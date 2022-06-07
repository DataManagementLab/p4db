#pragma once

#include "graph.hpp"
#include "partitioning.hpp"

#include <cstdint>
#include <tuple>


namespace declustered_layout {


struct GraphMaxCut {
    enum Algo {
        RMAXCUT,
        MQLIB,
    };

    static Partitioning part(Graph& graph, const Algo algo, uint32_t npart);


private:
    static std::tuple<Graph, Graph> split(Graph& g, Partitioning& part);

    static Partitioning part_rmaxcut(Graph& g);

    static Partitioning part_mqlib(Graph& g);
};


} // namespace declustered_layout