#pragma once

#include "db/types.hpp"

#include <cstdint>
#include <iostream>


using namespace std::chrono_literals;


// constexpr auto CC_SCHEME = CC_Scheme::NONE;
constexpr auto CC_SCHEME = CC_Scheme::NO_WAIT;
// constexpr auto CC_SCHEME = CC_Scheme::WAIT_DIE;

enum class StatsBitmask : uint64_t {
    NONE = 0x00,
    COUNTER = 0x01,
    CYCLES = 0x02,
    PERIODIC = 0x04,

    ALL = 0xffffffffffffffff,
};
constexpr StatsBitmask operator|(StatsBitmask lhs, StatsBitmask rhs) {
    using T = std::underlying_type<StatsBitmask>::type;
    return static_cast<StatsBitmask>(static_cast<T>(lhs) | static_cast<T>(rhs));
}
constexpr bool operator&(StatsBitmask lhs, StatsBitmask rhs) {
    using T = std::underlying_type<StatsBitmask>::type;
    return static_cast<T>(lhs) & static_cast<T>(rhs);
}

// constexpr StatsBitmask ENABLED_STATS = StatsBitmask::COUNTER | StatsBitmask::CYCLES | StatsBitmask::PERIODIC;

constexpr StatsBitmask ENABLED_STATS = StatsBitmask::CYCLES;
constexpr bool STATS_PER_WORKER = false;
constexpr auto STATS_CYCLE_SAMPLE_TIME = 10ms; //100us;
constexpr auto STATS_PERIODIC_SAMPLE_TIME = 500ms;
constexpr auto PERIODIC_CSV_FILENAME = "periodic.csv";
constexpr auto SINGLE_NUMA = false;


namespace error {

constexpr bool PRINT_ABORT_CAUSE = false;
constexpr bool LOG_TABLE = false;
constexpr bool DUMP_SWITCH_PKTS = false;

} // namespace error

// ALL
constexpr bool SWITCH_NO_CONFLICT = false;
constexpr bool LM_ON_SWITCH = false;
constexpr bool YCSB_OPTI_TEST = false;

// YCSB
// constexpr uint64_t NUM_KVS = 10'000'000;
constexpr int NUM_OPS = 8;
constexpr int MULTI_OP_PERCENTAGE = 100;
constexpr bool YCSB_SORT_ACCESSES = false;
constexpr bool YCSB_MULTI_MIX_RW = true; // set to false for fairness analysis

// SMALLBANK
constexpr int FREQUENCY_AMALGAMATE = 15;
constexpr int FREQUENCY_BALANCE = 15;
constexpr int FREQUENCY_DEPOSIT_CHECKING = 15;
constexpr int FREQUENCY_SEND_PAYMENT = 25;
constexpr int FREQUENCY_TRANSACT_SAVINGS = 15;
constexpr int FREQUENCY_WRITE_CHECK = 15;

// constexpr uint64_t NUM_ACCOUNTS = 1'000'000;

constexpr int MIN_BALANCE = 10000 * 100; // fixed-point instead of float
constexpr int MAX_BALANCE = 50000 * 100;

// TPCC
// constexpr uint64_t NUM_WAREHOUSES = 1;
// constexpr uint64_t HOME_W_ID = 0;
// constexpr uint64_t NUM_DISTRICTS = NUM_WAREHOUSES*DISTRICTS_PER_WAREHOUSE;

constexpr uint64_t DISTRICTS_PER_WAREHOUSE = 10;
constexpr uint64_t CUSTOMER_PER_DISTRICT = 3000;
constexpr uint64_t NUM_ITEMS = 100'000;

// How many orders contains each NewOrder Transaction?
constexpr uint64_t ORDER_CNT_MIN = 5;
// constexpr uint64_t ORDER_CNT_MAX = 15;
constexpr uint64_t ORDER_CNT_MAX = 10;

// access remote warehouse 1%
// constexpr int ORDER_REMOTE_WH_PROB = 1;
// constexpr int ORDER_REMOTE_WH_PROB = 10;

// whether paying customer is from remote wh and random district
// constexpr int PAYMENT_REMOTE_PROB = 15;

// NewOrder: 45%    Payment: 43%
constexpr int FREQUENCY_NEW_ORDER = 51;
// constexpr int FREQUENCY_NEW_ORDER = 100;