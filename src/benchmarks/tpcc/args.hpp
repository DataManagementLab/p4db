#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace benchmark {
namespace tpcc {


struct TPCCArgs {
    struct NewOrder {
        uint64_t w_id;
        uint64_t d_id;
        uint64_t c_id;

        NewOrder(uint64_t w_id, uint64_t d_id, uint64_t c_id) : w_id(w_id), d_id(d_id), c_id(c_id) {}

        static constexpr int MAX_ORDERS = 15;
        struct OrderItem {
            uint64_t ol_i_id;
            uint64_t ol_supply_w_id;
            uint32_t ol_quantity;
            bool is_hot;
        };
        static_assert(sizeof(OrderItem) == 24);
        size_t ol_cnt;                            // 5-15 orders, avg.: 10
        std::array<OrderItem, MAX_ORDERS> orders; // should be sorted to avoid deadlock, maybe use vector??

        bool on_switch = false;
    };

    struct Payment {
        uint64_t w_id;
        uint64_t d_id;
        uint64_t c_w_id;
        uint64_t c_d_id;
        uint64_t c_id;
        int64_t h_amount;

        bool on_switch = false;
    };


    using Arg_t = std::variant<NewOrder, Payment>;
};

} // namespace tpcc
} // namespace benchmark