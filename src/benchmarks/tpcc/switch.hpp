#pragma once

#include "args.hpp"
#include "comm/eth_hdr.hpp"
#include "db/buffers.hpp"
#include "declustered_layout/declustered_layout.hpp"


namespace benchmark {
namespace tpcc {

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
    uint8_t value;

    static constexpr uint8_t MAX_REG = 10;

    static constexpr auto SKIP() {
        return InstrType_t{0x00};
    }

    static constexpr auto STOP() {
        return InstrType_t{0x80};
    }

    static constexpr auto PAYMENT() {
        return InstrType_t{0x01};
    }

    static constexpr auto NEW_ORDER() {
        return InstrType_t{0x02};
    }

    static constexpr auto NO_STOCK(uint8_t id) {
        if (id >= MAX_REG) {
            throw std::invalid_argument("REG id out of bounds.");
        }
        return InstrType_t(uint8_t{0x03} + id);
    }

    auto reg_no_stock_idx() const {
        return value - uint8_t{0x03};
    }

    auto unset_stop() {
        return InstrType_t{static_cast<uint8_t>(value & ~STOP().value)};
    }

    auto set_stop(bool stop = true) {
        if (stop) {
            return InstrType_t{static_cast<uint8_t>(value | STOP().value)};
        }
        return *this;
    }

    auto split_stop() {
        bool is_stop = value & STOP().value;
        return std::make_pair(is_stop, this->unset_stop());
    }

    bool operator==(const InstrType_t& other) const {
        return value == other.value;
    }

    friend std::ostream& operator<<(std::ostream& os, const InstrType_t& self) {
        if (self == SKIP()) {
            os << "SKIP";
        } else if (self == STOP()) {
            os << "STOP";
        } else if (self == PAYMENT()) {
            os << "PAYMENT";
        } else if (self == NEW_ORDER()) {
            os << "NEW_ORDER";
        } else if (self.value < MAX_REG) {
            os << "NO_STOCK" << self.value - NO_STOCK(0).value;
        } else {
            throw std::invalid_argument("Could not convert: " + std::to_string(+self.value));
        }
        return os;
    }
};


struct payment_t {
    InstrType_t type = InstrType_t::PAYMENT();
    be_uint16_t w_id;
    be_uint16_t d_id;
    be_uint32_t h_amount;

    payment_t(be_uint16_t w_id, be_uint16_t d_id, be_uint32_t h_amount)
        : w_id(w_id), d_id(d_id), h_amount(h_amount) {}
} __attribute__((packed));


struct new_order_t {
    InstrType_t type = InstrType_t::NEW_ORDER();
    be_uint16_t d_id;
    be_uint32_t d_next_o_id;

    new_order_t(be_uint16_t d_id)
        : d_id(d_id) {}
} __attribute__((packed));


struct no_stock_t {
    InstrType_t type;
    be_uint16_t s_id;
    be_uint32_t ol_quantity;
    be_uint8_t is_remote;

    no_stock_t(InstrType_t type, be_uint16_t s_id, be_uint32_t ol_quantity, be_uint8_t is_remote)
        : type(type), s_id(s_id), ol_quantity(ol_quantity), is_remote(is_remote) {}
} __attribute__((packed));


struct TPCCDeclusteredLayout : public declustered_layout::DeclusteredLayout {
    using TupleLocation = declustered_layout::TupleLocation;

    static constexpr auto NUM_REGS = 10;   // max available registers, increase for overpartitioning
    static constexpr auto NUM_INSTRS = 15; // max available instructions


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


struct TPCCSwitchInfo {
    TPCCDeclusteredLayout declustered_layout;

    struct NewOrder {
        uint64_t d_id;
        TPCCArgs::NewOrder& args;

        uint16_t get_d_id() const {
            return static_cast<uint16_t>(d_id);
        }
    };


    struct Payment {
        uint64_t w_id;
        uint64_t d_id;
        int64_t h_amount;

        uint16_t get_w_id() const {
            return static_cast<uint16_t>(w_id);
        }

        uint16_t get_d_id() const {
            return static_cast<uint16_t>(d_id);
        }

        uint32_t get_h_amount() const {
            uint32_t val;
            std::memcpy(&val, &h_amount, sizeof(uint32_t)); // avoid sign conversion
            return val;
        }
    };


    void make_txn(const NewOrder& arg, BufferWriter& bw);
    void make_txn(const Payment& arg, BufferWriter& bw);

    struct NewOrderOut {
        uint32_t d_next_o_id;
    };
    struct PaymentOut {};

    NewOrderOut parse_txn(const NewOrder& arg, BufferReader& br);
    PaymentOut parse_txn(const Payment& arg, BufferReader& br);
};


} // namespace tpcc
} // namespace benchmark