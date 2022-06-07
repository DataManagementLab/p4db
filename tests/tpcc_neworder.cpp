#include "benchmarks/tpcc/random.hpp"
#include "benchmarks/tpcc/utils.hpp"
#include "db/defs.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>

// g++ -std=c++20 -g -O3 -march=native -pthread -I../src tpcc_neworder.cpp -o tpcc_neworder && ./tpcc_neworder


int main(void) {
    using namespace benchmark::tpcc;


    constexpr uint64_t SAMPLES = 5'000'000;


    uint64_t num_warehouses = 8;
    TPCCHotInfo hot_info{num_warehouses, 10 * (65536 / 2 + 32768 / 4)};

    {
        for (uint64_t w_id = 0; w_id < num_warehouses; ++w_id) {
            TPCCRandom rnd(w_id);
            uint64_t hot = 0;
            for (uint64_t i = 0; i < SAMPLES; ++i) {
                auto val = rnd.NURand<uint64_t>(8191, 1, NUM_ITEMS) - 1;
                hot += hot_info.is_hot(w_id, val);
            }
            std::cout << "w_id: " << w_id << " hot: " << (double)hot * 100 / SAMPLES << '\n';
        }
    }

    TPCCRandom rnd(0);
    for (uint64_t i = 0; i < 20; ++i) {
        uint64_t hot = 0;
        for (uint64_t j = 0; j < 10; ++j) {
            auto val = rnd.NURand<uint64_t>(8191, 1, NUM_ITEMS) - 1;
            hot += hot_info.is_hot(0, val);
        }
        std::cout << hot << '\n';
    }


    return 0;
}