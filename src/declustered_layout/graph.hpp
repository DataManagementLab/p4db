#pragma once

#include "db/util.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace declustered_layout {


template <typename From, typename To>
struct IncrementalLUT {
    std::unordered_map<From, To> map;
    std::unordered_map<To, From> mapr;
    To current_node_id = 0;

    To get(From tuple_id) {
        if (!map.contains(tuple_id)) {
            mapr[current_node_id] = tuple_id;
            map[tuple_id] = current_node_id;
            current_node_id++;
        }
        return map.at(tuple_id);
    }

    From rev(To node_id) const { return mapr.at(node_id); }

    void clear() {
        current_node_id = 0;
        map.clear();
        mapr.clear();
    }
};


struct Edge {
    uint64_t u;
    uint64_t v;

    static auto undirected(uint64_t u, uint64_t v) {
        return Edge{std::min(u, v), std::max(u, v)};
    }

    static auto directed(uint64_t u, uint64_t v) { return Edge{u, v}; }

    auto reverse() const { return Edge{v, u}; }

    friend bool operator==(const Edge& e1, const Edge& e2) {
        return (e1.u == e2.u) && (e1.v == e2.v);
    }

    struct hash {
        std::size_t operator()(const Edge& edge) const {
            // return ((std::size_t)edge.u << 32) | edge.v;
            return multi_hash(edge.u, edge.v);
        }
    };

private:
    Edge() = delete;
    Edge(uint64_t u, uint64_t v) : u(u), v(v) {}
};


struct Graph {
    IncrementalLUT<uint64_t, uint64_t> nid_lut;

    std::unordered_map<Edge, uint64_t, Edge::hash> undirected_ewgts;
    std::unordered_map<Edge, uint64_t, Edge::hash> directed_ewgts;
    std::unordered_set<uint64_t> dangling;

    void add_undirected_edge(uint64_t t1, uint64_t t2, uint64_t wgt = 1);

    Edge add_directed_edge(uint64_t t1, uint64_t t2, uint64_t wgt = 1);

    void remove_directed_edge(Edge& e);

    void add_dangling(uint64_t t);

    void print_stats();

    void clear();
};


} // namespace declustered_layout
