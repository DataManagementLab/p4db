#pragma once


#include "db/config.hpp"
#include "db/types.hpp"

#include <cstdint>
#include <tuple>
#include <utility>


enum class PartitionType {
    REPLICATED,
    RANGE,
    ROUND_ROBIN, // not implemented
    HASHED,      // not implemented
};

struct LocationInfo {
    bool is_local;
    msg::node_t target;
    bool is_hot;
    uint16_t abs_hot_index;
};


template <PartitionType type>
struct PartitionInfo;

template <>
struct PartitionInfo<PartitionType::REPLICATED> {
    PartitionInfo(const uint64_t) {}

    constexpr auto location(p4db::key_t index [[maybe_unused]]) {
        LocationInfo loc_info;
        loc_info.is_local = true;
        loc_info.target = msg::node_t{0}; // not used
        loc_info.is_hot = false;          // replicated, never on switch

        return loc_info;
    }

    // translate key directly to data-array index
    constexpr p4db::key_t translate(p4db::key_t index) {
        return index;
    }
};


template <>
struct PartitionInfo<PartitionType::RANGE> {
    const uint64_t total_size;
    uint64_t partition_size;
    uint64_t offset;
    uint64_t hot_size;
    msg::node_t my_id;
    msg::node_t switch_id;

    PartitionInfo(const uint64_t total_size)
        : total_size(total_size) {
        auto& config = Config::instance();

        if (total_size % config.num_nodes != 0) {
            throw std::runtime_error("total_size % num_nodes != 0");
        }

        my_id = config.node_id;
        partition_size = total_size / config.num_nodes;
        offset = partition_size * config.node_id;
        switch_id = config.switch_id;


        if constexpr (LM_ON_SWITCH) {
            // refactor
            switch (config.workload) {
                case BenchmarkType::YCSB:
                    hot_size = config.ycsb.hot_size;
                    break;
                // case BenchmarkType::SMALLBANK:
                //     hot_size = config.smallbank.hot_size;
                //     break;
                // case BenchmarkType::TPCC:
                //     hot_size = config.tpcc.hot_size;
                //     break;
                default:
                    throw std::runtime_error("workload not supported in partInfo for LM_ON_SWITCH");
            }
        }

        std::stringstream ss;
        ss << "partinfo_total_size=" << total_size << '\n';
        ss << "partinfo_partition_size=" << partition_size << '\n';
        ss << "partinfo_offset=" << offset << '\n';
        ss << "partinfo_my_id=" << my_id << '\n';
        std::cout << ss.str();
    }

    auto location(p4db::key_t index) {
        LocationInfo loc_info;
        loc_info.target = msg::node_t{static_cast<uint32_t>(index / partition_size)};
        loc_info.is_local = loc_info.target == my_id;

        if constexpr (LM_ON_SWITCH) {
            uint64_t local_idx = index - (loc_info.target * partition_size);
            loc_info.is_hot = local_idx < hot_size; // might be faster than % partition_size
            if (loc_info.is_hot) {
                loc_info.abs_hot_index = local_idx + (hot_size * loc_info.target);
                loc_info.target = switch_id;
                loc_info.is_local = false;
            } // enforce hot requests go to the switch
        }

        return loc_info;
    }

    p4db::key_t translate(p4db::key_t index) {
        // return p4db::key_t{index - offset};
        return index;
    }
};