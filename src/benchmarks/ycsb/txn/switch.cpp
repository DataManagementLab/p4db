#include "../switch.hpp"

namespace benchmark {
namespace ycsb {

void YCSBSwitchInfo::make_txn(const SingleRead& arg, BufferWriter& bw) {
    bw.write(info_t{});
    auto& tl = declustered_layout.get_location(arg.id);
    bw.write(instr_t{InstrType_t::REG(tl.stage_id), OPCode_t::READ, tl.reg_array_idx, 0x00000000});
    bw.write(InstrType_t::STOP());
}

void YCSBSwitchInfo::make_txn(const SingleWrite& arg, BufferWriter& bw) {
    bw.write(info_t{});
    auto& tl = declustered_layout.get_location(arg.id);
    bw.write(instr_t{InstrType_t::REG(tl.stage_id), OPCode_t::WRITE, tl.reg_array_id, arg.value});
    bw.write(InstrType_t::STOP());
}

void YCSBSwitchInfo::make_txn(const MultiOp& arg, BufferWriter& bw) {


    if (NUM_OPS != YCSBDeclusteredLayout::NUM_INSTRS) {
        throw std::invalid_argument("NUM_OPS != NUM_INSTR");
    }

    struct UniqInstrType_t {
        uint32_t id = 0;
        YCSBDeclusteredLayout::TupleLocation tl;
        YCSBArgs::Multi<NUM_OPS>::OP op; // might save some bytes if we only copy necessary fields

        bool operator<(const UniqInstrType_t& other) {
            if (id == other.id) {
                return tl.stage_id < other.tl.stage_id;
            }
            return id < other.id;
        }
    };

    std::array<UniqInstrType_t, YCSBDeclusteredLayout::NUM_INSTRS> accesses;
    std::array<uint32_t, YCSBDeclusteredLayout::NUM_REGS> cntr{};
    for (size_t i = 0; auto& op : arg.ops.ops) {
        auto& tl = declustered_layout.get_location(op.id);
        uint32_t id = cntr[tl.stage_id]++;
        accesses[i++] = UniqInstrType_t{id, tl, op};
    }
    std::sort(accesses.begin(), accesses.end());

    auto info = bw.write(info_t{});

    uint32_t nb_conflict = 0;
    lock_t locks{0, 0};
    for (int8_t last = -1; auto& access : accesses) {
        auto& tl = access.tl;
        auto& op = access.op;
        bool is_conflict = tl.stage_id <= last;
        if (tl.lock_bit == 0) { // left lock needed
            locks.left |= 1;
        } else {
            locks.right |= 1;
        }
        auto reg = InstrType_t::REG(tl.stage_id).set_stop(is_conflict);
        auto opcode = (op.mode == AccessMode::WRITE) ? OPCode_t::WRITE : OPCode_t::READ;
        bw.write(instr_t{reg, opcode, tl.reg_array_idx, op.value});
        nb_conflict += is_conflict;
        last = tl.stage_id;
    }
    bw.write(InstrType_t::STOP());

    // #warning "nb_conflict = 0"
    if constexpr (SWITCH_NO_CONFLICT) {
        nb_conflict = 0;
    }

    info->multipass = (nb_conflict > 0);
    info->locks = (nb_conflict > 0) ? locks : lock_t{0, 0};
    if constexpr (YCSB_OPTI_TEST) {
        // baseline test for fine-grained
        // info->locks = (nb_conflict > 0) ? lock_t{1, 1} : lock_t{0, 0};

        // 50% txn have left lock and 50% have right lock
        if (nb_conflict > 0) {
            info->locks =
                (arg.ops.ops[0].id % 2 == 0) ? lock_t{1, 0} : lock_t{0, 1};
        } else {
            info->locks = lock_t{0, 0};
        }
    }

    // {
    //     std::stringstream ss;
    //     BufferReader br{bw.buffer};
    //     auto info = br.template read<info_t>();
    //     ss << *info << '\n';
    //     for (auto i = 0; i < 8; ++i) {
    //         auto instr = br.template read<instr_t>();
    //         ss << *instr << '\n';
    //     }
    //     std::cout << ss.str();
    // }
}

YCSBSwitchInfo::SingleReadOut YCSBSwitchInfo::parse_txn(const SingleRead& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    auto instr = br.read<instr_t>();

    return SingleReadOut{*instr->data};
}

YCSBSwitchInfo::SingleWriteOut YCSBSwitchInfo::parse_txn(const SingleWrite& arg [[maybe_unused]], BufferReader& br [[maybe_unused]]) {
    return SingleWriteOut{};
}

YCSBSwitchInfo::MultiOpOut YCSBSwitchInfo::parse_txn(const MultiOp& arg [[maybe_unused]], BufferReader& br) {
    MultiOpOut out;
    for (int i = 0; i < NUM_OPS; ++i) {
        auto instr = br.read<instr_t>();
        out.values[i] = *instr->data;
    }
    return out;
}

} // namespace ycsb
} // namespace benchmark