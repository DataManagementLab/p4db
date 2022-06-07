#pragma once

#include "db/defs.hpp"
#include "db/types.hpp"

#include <cstdint>
#include <variant>


namespace benchmark {
namespace smallbank {


struct SmallbankArgs {
    struct Balance {
        uint64_t customer_id;
        bool on_switch;
    };
    struct DepositChecking {
        uint64_t customer_id;
        int32_t val;
        bool on_switch;
    };
    struct TransactSaving {
        uint64_t customer_id;
        int32_t val;
        bool on_switch;
    };
    struct Amalgamate {
        uint64_t customer_id_1;
        uint64_t customer_id_2;
        bool on_switch;
    };
    struct WriteCheck {
        uint64_t customer_id;
        int32_t val;
        bool on_switch;
    };
    struct SendPayment {
        uint64_t customer_id_1;
        uint64_t customer_id_2;
        int32_t val;
        bool on_switch;
    };


    using Arg_t = std::variant<Balance, DepositChecking, TransactSaving, Amalgamate, WriteCheck, SendPayment>;
};


} // namespace smallbank
} // namespace benchmark