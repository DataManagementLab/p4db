#pragma once

#include "graph.hpp"
#include "partitioning.hpp"

#include <cstdint>
#include <list>
#include <vector>


namespace declustered_layout {


struct GraphTopoSort {
    // Minimum weighted feedback arc set
    // opti: remove small-weight edges beforehand
    enum Algo {
        TIGHT,
        FAS,
        RANDOM,
    };

    Graph topo;
    std::vector<std::list<uint64_t>> adj_lut;
    uint64_t nnodes = 0;

    GraphTopoSort() = default;

    void setup(Graph& g, Partitioning& part);

    std::vector<uint64_t> topo_sort(const Algo algo);

private:
    std::vector<uint64_t> tight();


    std::vector<uint64_t> fas();

private:
    std::vector<uint64_t> tsort();

    bool has_cycle();


    bool has_cycle_until(uint64_t v, std::vector<bool>& visited, std::vector<bool>& recStack);
};


} // namespace declustered_layout