#pragma once


#include "db/defs.hpp"
#include "db/util.hpp"

#include <array>
#include <string_view>


namespace stats {

struct Counter {
    Counter();
    ~Counter();

    enum Name {
        local_read_lock_failed,
        local_write_lock_failed,
        local_lock_success,
        local_lock_waiting,
        remote_lock_failed,
        remote_lock_success,
        remote_lock_waiting,
        switch_aborts,

        tpcc_no_txns,
        tpcc_no_warehouse_read,
        tpcc_no_district_write,
        tpcc_no_customer_read,
        tpcc_no_warehouse_f_get,
        tpcc_no_district_f_get,
        tpcc_no_customer_f_get,
        tpcc_no_stock_acquired,
        tpcc_no_item_acquired,
        tpcc_no_commits,

        tpcc_pay_txns,
        tpcc_pay_warehouse_write,
        tpcc_pay_warehouse_read,
        tpcc_pay_district_write,
        tpcc_pay_district_read,
        tpcc_pay_customer_write,
        tpcc_pay_warehouse_f_get,
        tpcc_pay_district_f_get,
        tpcc_pay_customer_f_get,
        tpcc_pay_commits,

        micro_recirc_recircs,

        ycsb_read_commits,
        ycsb_write_commits,

        smallbank_amalgamate_commits,
        smallbank_balance_commits,
        smallbank_deposit_checking_commits,
        smallbank_send_payment_commits,
        smallbank_write_check_commits,
        smallbank_transact_saving_commits,

        __MAX
    };

    static constexpr std::array<std::string_view, __MAX> enum2str{
        "local_read_lock_failed",
        "local_write_lock_failed",
        "local_lock_success",
        "local_lock_waiting",
        "remote_lock_failed",
        "remote_lock_success",
        "remote_lock_waiting",
        "switch_aborts",

        "tpcc_no_txns",
        "tpcc_no_warehouse_read",
        "tpcc_no_district_write",
        "tpcc_no_customer_read",
        "tpcc_no_warehouse_f_get",
        "tpcc_no_district_f_get",
        "tpcc_no_customer_f_get",
        "tpcc_no_stock_acquired",
        "tpcc_no_item_acquired",
        "tpcc_no_commits",

        "tpcc_pay_txns",
        "tpcc_pay_warehouse_write",
        "tpcc_pay_warehouse_read",
        "tpcc_pay_district_write",
        "tpcc_pay_district_read",
        "tpcc_pay_customer_write",
        "tpcc_pay_warehouse_f_get",
        "tpcc_pay_district_f_get",
        "tpcc_pay_customer_f_get",
        "tpcc_pay_commits",

        "micro_recirc_recircs",

        "ycsb_read_commits",
        "ycsb_write_commits",

        "smallbank_amalgamate_commits",
        "smallbank_balance_commits",
        "smallbank_deposit_checking_commits",
        "smallbank_send_payment_commits",
        "smallbank_write_check_commits",
        "smallbank_transact_saving_commits",
    };

    // std::atomic<uint64_t> counters[__MAX]{};
    std::array<uint64_t, __MAX> counters{};


#define __forceinline inline __attribute__((always_inline))

    __forceinline void incr(const Name name) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::COUNTER)) {
            return;
        }
        auto& cntr = counters[name];
        // auto local = cntr.load();
        // ++local;
        // cntr.store(local, std::memory_order_relaxed);
        ++cntr;
    }

    __forceinline void incr(const Name name, auto amount) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::COUNTER)) {
            return;
        }
        auto& cntr = counters[name];
        cntr += amount;
    }
};

} // namespace stats
