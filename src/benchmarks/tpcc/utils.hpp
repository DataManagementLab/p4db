#pragma once


#include "db/defs.hpp"
#include "random.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>


struct TPCCHotInfo {
    struct Entry {
        uint64_t idx;
        uint64_t hits = 0;

        bool operator<(const Entry& other) {
            return hits > other.hits;
        }
    };

    using info_array_t = std::array<bool, NUM_ITEMS>;
    std::unique_ptr<info_array_t[]> info;
    uint64_t num_warehouses;

    static constexpr uint64_t SAMPLES = 5'000'000;

    TPCCHotInfo(uint64_t num_warehouses, uint64_t switch_slots) : num_warehouses(num_warehouses) {

        info = std::make_unique<info_array_t[]>(num_warehouses);

        uint64_t hot_items = switch_slots / num_warehouses;
        for (uint64_t w_id = 0; w_id < num_warehouses; ++w_id) {
            benchmark::tpcc::TPCCRandom rnd(w_id);

            std::array<Entry, NUM_ITEMS> cntr{};
            for (size_t i = 0; i < cntr.size(); ++i) {
                cntr[i].idx = i;
            }

            for (size_t i = 0; i < SAMPLES; ++i) {
                auto val = rnd.NURand<uint64_t>(8191, 1, NUM_ITEMS) - 1;
                cntr[val].hits++;
            }

            std::sort(cntr.begin(), cntr.end());

            for (size_t i = 0; i < NUM_ITEMS; i++) {
                info[w_id][cntr[i].idx] = (i < hot_items);
            }
        }
    }

    // ~TPCCHotInfo() {
    //     for (uint64_t w_id = 0; w_id < num_warehouses; ++w_id) {
    //         uint64_t num_hot = 0;
    //         std::array<Entry, NUM_ITEMS> cntr{};
    //         for (size_t i_id = 0; i_id < NUM_ITEMS; ++i_id) {
    //             num_hot += is_hot(w_id, i_id);
    //         }
    //         std::cout << "num_hot=" << num_hot << '\n';
    //     }
    // }

    bool is_hot(uint64_t w_id, uint64_t i_id) {
        if (!(w_id < num_warehouses && i_id < NUM_ITEMS)) {
            return false;
        }
        return info[w_id][i_id];
    }
};