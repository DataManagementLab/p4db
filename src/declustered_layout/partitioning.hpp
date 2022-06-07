#pragma once

#include "db/util.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>


namespace declustered_layout {


struct Partitioning {
    using map_t = std::unordered_map<uint64_t, uint64_t>;

    uint64_t parts;
    map_t map;

    Partitioning(uint64_t parts);

    void insert(uint64_t tid, uint64_t pid);

    bool has(uint64_t tid) const;

    const uint64_t& get(const uint64_t tid) const;

    Partitioning& operator+=(const Partitioning& rhs);

    void print();

    void print_stats();
};


} // namespace declustered_layout
