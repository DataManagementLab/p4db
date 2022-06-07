#pragma once

#include "args.hpp"
#include "comm/eth_hdr.hpp"
#include "db/buffers.hpp"
#include "declustered_layout/declustered_layout.hpp"

#include <iostream>
#include <stdexcept>


namespace benchmark {
namespace ycsb {


struct lock_t {
    uint8_t left = 0;
    uint8_t right = 0;
} __attribute__((packed));

struct info_t {
    be_uint8_t multipass;
    be_uint32_t recircs = 0;
    lock_t locks;

    friend std::ostream& operator<<(std::ostream& os, const info_t& self) {
        os << "multipass=" << self.multipass << " recircs=" << self.recircs << " locks=" << +self.locks.left << ',' << +self.locks.right;
        return os;
    }
} __attribute__((packed));

struct InstrType_t {
    uint8_t value;

    static constexpr uint8_t MAX_REG = 40;

    static constexpr auto SKIP() {
        return InstrType_t{0x00};
    }

    static constexpr auto STOP() {
        return InstrType_t{0x80};
    }

    static constexpr auto REG(uint8_t id) {
        if (id >= MAX_REG) {
            throw std::invalid_argument("REG id out of bounds.");
        }
        return InstrType_t(uint8_t{1} + id);
    }

    auto reg_idx() const {
        if (*this == SKIP() || *this == STOP() || value > MAX_REG) { // value -1
            throw std::invalid_argument("Does not contain register value: " + std::to_string(value));
        }
        return value - uint8_t{1};
    }

    auto unset_stop() const {
        return InstrType_t{static_cast<uint8_t>(value & ~STOP().value)};
    }

    auto set_stop(bool stop = true) {
        if (stop) {
            return InstrType_t{static_cast<uint8_t>(value | STOP().value)};
        }
        return *this;
    }

    bool operator==(const InstrType_t& other) const {
        return value == other.value;
    }

    friend std::ostream& operator<<(std::ostream& os, const InstrType_t& self) {
        if (self == SKIP()) {
            os << "SKIP";
        } else if (self == STOP()) {
            os << "STOP";
        } else if (self.unset_stop().value < MAX_REG + 1) {
            bool is_stop = self.value & STOP().value;
            os << "REG" << (is_stop ? '*' : '_') << self.unset_stop().value - 1;
        } else {
            throw std::invalid_argument("Could not convert: " + std::to_string(+self.value));
        }
        return os;
    }
};


enum class OPCode_t : uint8_t {
    READ = 0x00,
    WRITE = 0x01,
};
inline std::ostream& operator<<(std::ostream& os, const OPCode_t& self) {
    switch (self) {
        case OPCode_t::READ:
            return os << 'R';
        case OPCode_t::WRITE:
            return os << 'W';
        default:
            return os << "Unknown";
    }
}

struct instr_t {
    InstrType_t type;
    OPCode_t op;
    be_uint16_t idx;
    be_uint32_t data;

    instr_t(InstrType_t type, OPCode_t op, be_uint16_t idx, be_uint32_t data)
        : type(type), op(op), idx(idx), data(data) {}

    friend std::ostream& operator<<(std::ostream& os, const instr_t& self) {
        os << "type=" << self.type << " op=" << self.op << " idx=" << self.idx << " data=" << self.data;
        return os;
    }
} __attribute__((packed));


struct YCSBDeclusteredLayout : public declustered_layout::DeclusteredLayout {
    using TupleLocation = declustered_layout::TupleLocation;

    static constexpr auto NUM_REGS = 20;  // max available registers, increase for overpartitioning
    static constexpr auto NUM_INSTRS = 8; // 10; // max available instructions


    TupleLocation tl;

    const TupleLocation& get_location(uint64_t idx) {
        // simulate random layout, if not required comment this function out
        tl.stage_id = idx % NUM_REGS;
        tl.reg_array_id = 0; // unused for now
        tl.reg_array_idx = static_cast<uint16_t>(idx);
        tl.lock_bit = tl.stage_id < NUM_REGS / 2;
        return tl;
    }
};


struct YCSBSwitchInfo {
    YCSBDeclusteredLayout declustered_layout;

    struct SingleRead {
        uint64_t id;
    };

    struct SingleWrite {
        uint64_t id;
        uint32_t value;
    };

    struct MultiOp {
        YCSBArgs::Multi<NUM_OPS>& ops;
    };

    void make_txn(const SingleRead& arg, BufferWriter& bw);
    void make_txn(const SingleWrite& arg, BufferWriter& bw);
    void make_txn(const MultiOp& arg, BufferWriter& bw);

    struct SingleReadOut {
        uint32_t value;
    };
    struct SingleWriteOut {};
    struct MultiOpOut {
        std::array<uint32_t, NUM_OPS> values;
    };

    SingleReadOut parse_txn(const SingleRead& arg, BufferReader& br);
    SingleWriteOut parse_txn(const SingleWrite& arg, BufferReader& br);
    MultiOpOut parse_txn(const MultiOp& arg, BufferReader& br);
};

} // namespace ycsb
} // namespace benchmark
