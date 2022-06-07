#pragma once

#include "args.hpp"
#include "comm/eth_hdr.hpp"
#include "db/buffers.hpp"
#include "declustered_layout/declustered_layout.hpp"


namespace benchmark {
namespace smallbank {

// Switch specific

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
    union {
        uint8_t val = 0;
        struct {
            uint8_t num : 4;
            uint8_t id : 2;
            uint8_t skip : 1;
            uint8_t stop : 1;
        } bitwise; // does not work to set manually __attribute__ ((reverse_bitfields));
    };

    static constexpr uint8_t MAX_REG = 10;


    static constexpr auto BALANCE(uint8_t num) {
        InstrType_t type;
        type.bitwise.id = 0b00;
        type.bitwise.num = num;
        return type;
    }

    static constexpr auto DEPOSIT_CHECKING(uint8_t num) {
        InstrType_t type;
        type.bitwise.id = 0b01;
        type.bitwise.num = num;
        return type;
    }

    static constexpr auto TRANSACT_SAVING(uint8_t num) {
        InstrType_t type;
        type.bitwise.id = 0b10;
        type.bitwise.num = num;
        return type;
    }

    static constexpr auto AMALGAMATE(uint8_t num) {
        InstrType_t type;
        type.bitwise.id = 0b11;
        type.bitwise.num = num;
        return type;
    }

    static constexpr auto AMALGAMATE_EGRESS() {
        return InstrType_t{0}; // unused for now
    }
    static constexpr auto SEND_PAYMENT_EGRESS() {
        return InstrType_t{0}; // unused for now
    }

    static constexpr auto WRITE_CHECK_EGRESS() {
        return InstrType_t{0}; // unused for now
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
                os << "BAL[" << +self.bitwise.num << "]";
                break;
            case 0b01:
                os << "DEP[" << +self.bitwise.num << "]";
                break;
            case 0b10:
                os << "TRS[" << +self.bitwise.num << "]";
                break;
            case 0b11:
                os << "AML[" << +self.bitwise.num << "]";
                break;
        }
        return os;
    }
} __attribute__((packed));
static_assert(sizeof(InstrType_t) == 1);


struct balance_t {
    InstrType_t type;
    be_uint16_t c_id;
    be_uint32_t saving_bal;
    be_uint32_t checking_bal;

    friend std::ostream& operator<<(std::ostream& os, const balance_t& self) {
        os << "type=" << self.type << " c_id=" << self.c_id << " saving_bal=" << self.saving_bal << " checking_bal=" << self.checking_bal;
        return os;
    }
} __attribute__((packed));


struct deposit_checking_t {
    InstrType_t type;
    be_uint16_t c_id;
    be_uint32_t checking_bal;

    friend std::ostream& operator<<(std::ostream& os, const deposit_checking_t& self) {
        os << "type=" << self.type << " c_id=" << self.c_id << " checking_bal=" << self.checking_bal;
        return os;
    }
} __attribute__((packed));


struct transact_saving_t {
    InstrType_t type;
    be_uint16_t c_id;
    be_uint32_t saving_bal;

    friend std::ostream& operator<<(std::ostream& os, const transact_saving_t& self) {
        os << "type=" << self.type << " c_id=" << self.c_id << " saving_bal=" << self.saving_bal;
        return os;
    }
} __attribute__((packed));


struct amalgamate_t {
    InstrType_t type; // DON'T set to SKIP after processing
    be_uint16_t c_id_0;
    be_uint32_t saving_bal;
    be_uint32_t checking_bal;
} __attribute__((packed));


struct write_check_egress_t {
    InstrType_t type;
    be_uint16_t c_id;
    be_uint32_t balance;

    friend std::ostream& operator<<(std::ostream& os, const write_check_egress_t& self) {
        os << "type=" << self.type << " c_id=" << self.c_id << " balance=" << self.balance;
        return os;
    }
} __attribute__((packed));


struct amalgamate_egress_t {
    InstrType_t type; // set to SKIP after processing
    InstrType_t c_type;
    be_uint16_t c_id_1;
} __attribute__((packed));


struct send_payment_egress_t {
    InstrType_t type; // set to SKIP after processing
    InstrType_t c_type_0;
    InstrType_t c_type_1;
    be_uint16_t c_id_1;
    be_uint32_t balance;
} __attribute__((packed));


struct abort_t {
    InstrType_t type;
} __attribute__((packed));


struct SmallbankDeclusteredLayout : public declustered_layout::DeclusteredLayout {
    using TupleLocation = declustered_layout::TupleLocation;

    static constexpr auto NUM_REGS = 10;


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


struct SmallbankSwitchInfo {
    SmallbankDeclusteredLayout declustered_layout;


    struct Amalgamate {
        uint64_t c_id_0;
        uint64_t c_id_1;
    };

    struct Balance {
        uint64_t c_id;
    };

    struct DepositChecking {
        uint64_t c_id;
        int32_t amount;
    };

    struct SendPayment {
        uint64_t c_id_0;
        uint64_t c_id_1;
        int32_t amount;
    };

    struct TransactSaving {
        uint64_t c_id;
        int32_t amount;
    };

    struct WriteCheck {
        uint64_t c_id;
        int32_t amount;
    };


    void make_txn(const Amalgamate& arg, BufferWriter& bw);
    void make_txn(const Balance& arg, BufferWriter& bw);
    void make_txn(const DepositChecking& arg, BufferWriter& bw);
    void make_txn(const SendPayment& arg, BufferWriter& bw);
    void make_txn(const TransactSaving& arg, BufferWriter& bw);
    void make_txn(const WriteCheck& arg, BufferWriter& bw);


    struct AmalgamateOut {
        uint32_t checking_bal;
    };

    struct BalanceOut {
        uint32_t saving_bal;
        uint32_t checking_bal;
    };

    struct DepositCheckingOut {
        uint32_t checking_bal;
    };

    struct SendPaymentOut {
        bool abort = false;
        uint32_t checking_bal_0;
        uint32_t checking_bal_1;
    };

    struct TransactSavingOut {
        uint32_t saving_bal;
    };

    struct WriteCheckOut {
        uint32_t checking_bal;
    };


    AmalgamateOut parse_txn(const Amalgamate& arg, BufferReader& br);
    BalanceOut parse_txn(const Balance& arg, BufferReader& br);
    DepositCheckingOut parse_txn(const DepositChecking& arg, BufferReader& br);
    SendPaymentOut parse_txn(const SendPayment& arg, BufferReader& br);
    TransactSavingOut parse_txn(const TransactSaving& arg, BufferReader& br);
    WriteCheckOut parse_txn(const WriteCheck& arg, BufferReader& br);
};

} // namespace smallbank
} // namespace benchmark