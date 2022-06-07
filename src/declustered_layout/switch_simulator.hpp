#pragma once

#include "declustered_layout.hpp"
#include "partitioning.hpp"
#include "transaction.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <vector>


namespace declustered_layout {


struct SwitchSimulator {
    DeclusteredLayout& dcl;
    bool include_deps = false;

    SwitchSimulator(DeclusteredLayout& dcl);

    void process(std::vector<Transaction> txns);
};


} // namespace declustered_layout
