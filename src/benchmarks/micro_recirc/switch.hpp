#pragma once


#include "args.hpp"
#include "comm/eth_hdr.hpp"
#include "db/buffers.hpp"

namespace benchmark {
namespace micro_recirc {

// Switch specific

struct lock_t {
    uint8_t left = 0;
    uint8_t right = 0;
} __attribute__((packed));

struct info_t {
    be_uint8_t multipass = 0;
    be_uint32_t recircs = 0;
    lock_t locks{};

    friend std::ostream& operator<<(std::ostream& os, const info_t& self) {
        os << "multipass=" << self.multipass << " recircs=" << self.recircs << " locks=" << +self.locks.left << ',' << +self.locks.right;
        return os;
    }
} __attribute__((packed));


struct InstrType_t {
    union {
        uint8_t val = 0;
        struct {
            uint8_t num : 4;
            uint8_t id : 2;
            uint8_t skip : 1;
            uint8_t stop : 1;
        } bitwise; // does not work to set manually __attribute__ ((reverse_bitfields));
    };


    static constexpr auto RECIRC() {
        InstrType_t type;
        type.bitwise.id = 0b00;
        type.bitwise.num = 0b01;
        return type;
    }

    static constexpr auto SKIP() {
        InstrType_t type;
        type.bitwise.skip = true;
        return type;
    }

    static constexpr auto ABORT() {
        InstrType_t type;
        type.bitwise.stop = true;
        return type;
    }

    static constexpr auto STOP() {
        InstrType_t type;
        type.bitwise.stop = true;
        return type;
    }

    auto unset_stop() {
        bitwise.stop = false;
        return *this;
    }

    auto set_stop(bool stop = true) {
        bitwise.stop = stop;
        return *this;
    }

    bool operator==(const InstrType_t& other) const {
        return val == other.val;
    }

    friend std::ostream& operator<<(std::ostream& os, const InstrType_t& self) {
        os << (self.bitwise.skip ? '*' : '-');
        os << (self.bitwise.stop ? '*' : '-');

        switch (self.bitwise.id) {
            case 0b00:
                os << "RECIRC[" << +self.bitwise.num << "]";
                break;
        }
        return os;
    }
} __attribute__((packed));
static_assert(sizeof(InstrType_t) == 1);


struct recirc_t {
    InstrType_t type = InstrType_t::RECIRC();

    friend std::ostream& operator<<(std::ostream& os, const recirc_t& self) {
        os << "type=" << self.type;
        return os;
    }
} __attribute__((packed));


struct MicroRecircSwitchInfo {
    struct Recirc {
        uint32_t recircs;

        be_uint32_t get_recircs() const {
            return recircs;
        }
    };


    void make_txn(const Recirc& arg, BufferWriter& bw);

    struct RecircOut {};

    RecircOut parse_txn(const Recirc& arg, BufferReader& br);
};


} // namespace micro_recirc
} // namespace benchmark