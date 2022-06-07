#include "../switch.hpp"

#include "../table.hpp"


namespace benchmark {
namespace micro_recirc {


void MicroRecircSwitchInfo::make_txn(const Recirc& arg, BufferWriter& bw) {
    constexpr auto NUM_INSTR = 15;
    if (arg.recircs > NUM_INSTR - 1) {
        throw std::runtime_error("increase NUM_INSTR");
    }


    auto info = bw.write(info_t{});
    bw.write(recirc_t{});

    for (uint32_t i = 0; i < arg.recircs; ++i) {
        auto instr = bw.write(recirc_t{});
        instr->type.set_stop(true);
        // instr->type.bitwise.stop = true; // In case compiler removes call
    }
    if (arg.recircs > 0) {
        info->multipass = 1;
        info->locks = lock_t{1, 1};
    }

    bw.write(InstrType_t::STOP());
}


MicroRecircSwitchInfo::RecircOut MicroRecircSwitchInfo::parse_txn(const Recirc& arg [[maybe_unused]], BufferReader& br [[maybe_unused]]) {
    auto info = br.read<info_t>();

    WorkerContext::get().cntr.incr(stats::Counter::micro_recirc_recircs, *info->recircs);

    return RecircOut{};
}

} // namespace micro_recirc
} // namespace benchmark