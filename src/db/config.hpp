#pragma once


#include "comm/comm.hpp"
#include "comm/server.hpp"
#include "db/defs.hpp"
#include "db/util.hpp"

#include <vector>


class Config : public HeapSingleton<Config> {
    friend class HeapSingleton<Config>;


protected:
    Config() = default;

public:
    void parse_cli(int argc, char** argv);
    void print();


    std::vector<Server> servers = {};

    msg::node_t node_id;
    uint32_t num_nodes;
    uint32_t num_txn_workers;
    msg::node_t switch_id;
    uint64_t switch_entries;

    BenchmarkType workload;
    uint64_t num_txns;
    bool use_switch;
    bool verify;
    std::string csv_file_cycles{"cycles.csv"};

    struct YCSB {
        uint64_t table_size;
        int write_prob;
        int remote_prob;
        uint64_t hot_size;
        int hot_prob;
    } ycsb;

    struct Smallbank {
        uint64_t table_size;
        uint64_t hot_size;
        int hot_prob;
        int remote_prob;
    } smallbank;

    struct TPCC {
        uint64_t num_warehouses;
        uint64_t num_districts;
        uint64_t home_w_id;
        int new_order_remote_prob = 1; // default by tpcc-spec ( = 10 for test)
        int payment_remote_prob = 15;  // default by tpcc-spec
    } tpcc;

    struct MicroRecirc {
        int recirc_prob;
    } micro_recirc;
};
