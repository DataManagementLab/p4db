#pragma once

#include "graph.hpp"
#include "transaction.hpp"
#include "tuple_location.hpp"

#include <cstdint>
#include <unordered_map>


namespace declustered_layout {


struct DeclusteredLayout {
    // Intel confidential
    static constexpr auto STAGES = 42;
    static constexpr auto REGS_PER_STAGE = 1;
    static constexpr auto REG_SIZE = 42;
    static constexpr auto LOCK_BITS = 2;
    static constexpr auto PARTITIONS = STAGES * REGS_PER_STAGE;
    static constexpr auto MAX_ACCESSES = 10;


    Graph g;
    std::unordered_map<uint64_t, TupleLocation> switch_tuples;

    void add_sample(Transaction& txn);

    void compute_layout(bool topo_sort, bool write_dot);

    bool is_hot(uint64_t idx) const;

    const TupleLocation& get_location(uint64_t idx) const;

    void clear();

    void print();
};


} // namespace declustered_layout
