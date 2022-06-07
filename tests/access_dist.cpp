#include "utils/dist.hpp"
#include "utils/zipf.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>


enum class LockMode {
    SHARED = 0,
    EXCLUSIVE = 1,
};


struct lockid_mode_pair_t {
    uint64_t id;
    LockMode mode;

    bool operator<(const lockid_mode_pair_t& rhs) const {
        return id < rhs.id;
    }
};


class Transaction {
public:
    Transaction() = default;
    bool on_switch;
    std::vector<lockid_mode_pair_t> requests;
};


struct Partition {
    uint64_t start;
    uint64_t end;

    constexpr bool contains(uint64_t n) {
        return start <= n && n < end;
    }
};

struct PartitionGenerator {
    uint64_t start;
    uint64_t size;

    PartitionGenerator(uint64_t start, uint64_t size) : start(start), size(size) {}

    Partition operator()() {
        Partition r{start, start + size - 1};
        start += size;
        return r;
    }
};


template <typename Config>
auto get_txns(Config* config) {
    std::random_device rd;
    std::mt19937 gen(config->my_rank);

    uint64_t hot_size = config->use_switch ? config->hot_size : config->hot_size / config->total_table_agents; // if no switch used, split hot set across nodes

    PercentDist is_hot(config->hot_prob);
    PercentDist is_remote(config->remote_prob);
    PercentDist is_write(config->write_prob);
    UniformRemoteRank remote_rank_dist{config->total_table_agents, config->my_rank};

    std::uniform_int_distribution<uint64_t> hot_dist{0, hot_size - 1}; // so that it can fit on the switch
    std::uniform_int_distribution<uint64_t> cold_dist{hot_size, config->partition_size - 1};

    auto& partitions = config->partitions;
    /*Partition& local_partition =*/partitions.at(config->my_rank);

    std::vector<Transaction> transactions;
    transactions.reserve(config->num_txn);
    for (uint64_t i = 0; i < config->num_txn; ++i) {
        Transaction& transaction = transactions.emplace_back();
        transaction.requests.reserve(config->locks_per_txn);

        auto add_request = [&](uint64_t lock_idx, LockMode mode) {
            bool is_duplicate = std::any_of(transaction.requests.begin(), transaction.requests.end(), [&](auto& req) {
                return req.id == lock_idx;
            });
            if (!is_duplicate) {
                transaction.requests.emplace_back(lock_idx, mode);
            }
        };

        bool is_hot_txn = is_hot(gen);

        if (!config->use_switch) { // case 1
            while (transaction.requests.size() < config->locks_per_txn) {
                uint32_t target_rank = is_remote(gen) ? remote_rank_dist(gen) : config->my_rank;

                uint64_t lock_idx = is_hot_txn ? hot_dist(gen) : cold_dist(gen);
                lock_idx += partitions.at(target_rank).start;

                LockMode mode = is_write(gen) ? LockMode::EXCLUSIVE : LockMode::SHARED;
                add_request(lock_idx, mode);
            }

        } else { // case 2 and 3

            if (is_hot_txn) {                                  // hot txn always goes to switch
                transaction.on_switch = config->switch_atomic; // should switch do 2pl/2pc or atomic ?
                while (transaction.requests.size() < config->locks_per_txn) {
                    uint64_t lock_idx = partitions.at(config->switch_rank).start + hot_dist(gen);
                    LockMode mode = is_write(gen) ? LockMode::EXCLUSIVE : LockMode::SHARED;
                    add_request(lock_idx, mode);
                }
            } else { // cold_txn goes only to the local partition or remote node-partitions
                while (transaction.requests.size() < config->locks_per_txn) {
                    uint32_t target_rank = is_remote(gen) ? remote_rank_dist(gen) : config->my_rank;
                    uint64_t lock_idx = partitions.at(target_rank).start + cold_dist(gen);
                    LockMode mode = is_write(gen) ? LockMode::EXCLUSIVE : LockMode::SHARED;
                    add_request(lock_idx, mode);
                }
            }
        }
    }

    return transactions;
}


struct Config {
    uint32_t my_rank = 0;
    uint32_t total_table_agents = 8;
    uint32_t partition_size = 1024;
    uint32_t remote_prob = 20;
    uint32_t write_prob = 40;
    uint32_t num_txn = 10'000;
    uint32_t locks_per_txn = 10;
    bool use_switch = true;
    bool switch_atomic = false;
    uint32_t switch_rank = 0;
    uint32_t hot_size = 80;
    uint32_t hot_prob = 50; // TODO also for 40 and compare
    std::vector<Partition> partitions;
};


int main(void) {
    // g++ -std=c++20 -g -O3 -march=native access_dist.cpp -o access_dist && ./access_dist

    std::ofstream access_dist;
    access_dist.open("access_dist.csv");
    access_dist << "total_table_agents,remote_prob,write_prob,hot_prob,target_rank,lock_id,mode,hot_size\n";

    for (auto write_prob : {10, 20, 40, 60}) {
        for (auto hot_prob : {40, 90}) {
            struct Config config;
            config.write_prob = write_prob;
            config.hot_prob = hot_prob;
            if (config.use_switch) {
                config.switch_rank = config.total_table_agents;
            }
            uint32_t num_servers = config.total_table_agents + config.use_switch;
            config.partitions.reserve(num_servers);
            std::generate_n(std::back_inserter(config.partitions), num_servers, PartitionGenerator(0, config.partition_size));

            auto transactions = get_txns(&config);

            std::map<uint32_t, uint64_t> vote_span;

            for (auto& txn : transactions) {
                for (auto& [lock_id, mode] : txn.requests) {
                    uint32_t remote_rank = lock_id / config.partition_size;

                    access_dist
                        << config.total_table_agents << ','
                        << config.remote_prob << ','
                        << config.write_prob << ','
                        << config.hot_prob << ','
                        << remote_rank << ','
                        << (lock_id % config.partition_size) << ','
                        << (mode == LockMode::SHARED ? "SH" : "EX") << ','
                        << config.hot_size << '\n';
                }
            }
        }
    }

    return 0;
}