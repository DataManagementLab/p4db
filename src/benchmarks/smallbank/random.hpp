#pragma once

#include "db/config.hpp"
#include "db/defs.hpp"

#include <random>


namespace benchmark {
namespace smallbank {

class SmallbankRandom {
public:
    using RandomDevice = std::mt19937;

private:
    RandomDevice gen;
    Config& config;
    const uint64_t local_part_size;

public:
    SmallbankRandom(uint32_t seed)
        : gen(seed), config(Config::instance()), local_part_size(config.smallbank.table_size / config.num_nodes) {}

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
        return random<int>(1, 100) <= config.smallbank.hot_prob;
    }
    bool is_remote() {
        return random<int>(1, 100) <= config.smallbank.remote_prob;
    }

    auto hot_cid() {
        auto id = random<uint64_t>(0, config.smallbank.hot_size - 1);
        if (is_remote()) {
            id += random_except<uint64_t>(0, config.num_nodes - 1, config.node_id) * local_part_size;
        } else {
            id += config.node_id * local_part_size;
        }
        return id;
    }
    auto cold_cid() {
        auto id = random<uint64_t>(config.smallbank.hot_size, local_part_size - config.smallbank.hot_size);
        if (is_remote()) {
            id += random_except<uint64_t>(0, config.num_nodes - 1, config.node_id) * local_part_size;
        } else {
            id += config.node_id * local_part_size;
        }
        return id;
    }

    template <typename T>
    T balance(T min = MIN_BALANCE, T max = MAX_BALANCE) {
        std::normal_distribution<double> dist;

        const T range = max - min;
        while (true) {
            double gaussian = (dist(gen) + 2.0) / 4.0;
            T value = std::round(gaussian * range);
            if (value < 0 || value > range) {
                continue;
            }
            return value + min;
        }
    }
};


} // namespace smallbank
} // namespace benchmark