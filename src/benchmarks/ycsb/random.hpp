#pragma once

#include "db/config.hpp"
#include "db/defs.hpp"

#include <random>


namespace benchmark {
namespace ycsb {

class YCSBRandom {
public:
    using RandomDevice = std::mt19937;
    RandomDevice gen;

private:
    Config& config;
    const uint64_t local_part_size;

public:
    YCSBRandom(uint32_t seed)
        : gen(seed), config(Config::instance()), local_part_size(config.ycsb.table_size / config.num_nodes) {}

    template <typename T>
    T random(T lower, T upper) {
        std::uniform_int_distribution<T> dist(lower, upper);
        return dist(gen);
    }

    template <typename T>
    T random_except(T lower, T upper, T except) {
        std::uniform_int_distribution<T> dist(lower, upper - 1);
        auto num = dist(gen);
        if (num == except) {
            return upper;
        }
        return num;
    }


    bool is_hot_txn() {
        return random<int>(1, 100) <= config.ycsb.hot_prob;
    }
    bool is_write() {
        return random<int>(1, 100) <= config.ycsb.write_prob;
    }
    bool is_multi() {
        return random<int>(1, 100) <= MULTI_OP_PERCENTAGE;
    }
    bool is_remote() {
        return random<int>(1, 100) <= config.ycsb.remote_prob;
    }

    auto hot_id() {
        auto id = random<uint64_t>(0, config.ycsb.hot_size - 1);
        if (is_remote()) {
            id += random_except<uint64_t>(0, config.num_nodes - 1, config.node_id) * local_part_size;
        } else {
            id += config.node_id * local_part_size;
        }
        return id;
    }
    auto cold_id() {
        auto id = random<uint64_t>(config.ycsb.hot_size, local_part_size - config.ycsb.hot_size - 1);
        if (is_remote()) {
            id += random_except<uint64_t>(0, config.num_nodes - 1, config.node_id) * local_part_size;
        } else {
            id += config.node_id * local_part_size;
        }
        return id;
    }

    template <typename T>
    T value() {
        std::uniform_int_distribution<T> dist; // 0 ... T_max
        return dist(gen);
    }
};

} // namespace ycsb
} // namespace benchmark