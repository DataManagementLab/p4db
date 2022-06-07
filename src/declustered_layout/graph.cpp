#include "graph.hpp"


namespace declustered_layout {


void Graph::add_undirected_edge(uint64_t t1, uint64_t t2, uint64_t wgt) {
    auto edge = Edge::undirected(nid_lut.get(t1), nid_lut.get(t2));
    undirected_ewgts[edge] += wgt;
}

Edge Graph::add_directed_edge(uint64_t t1, uint64_t t2, uint64_t wgt) {
    auto edge = Edge::directed(nid_lut.get(t1), nid_lut.get(t2));
    directed_ewgts[edge] += wgt;
    return edge;
}

void Graph::remove_directed_edge(Edge& e) {
    auto it = directed_ewgts.find(e);
    if (it == directed_ewgts.end()) {
        throw std::runtime_error("Could not find directed edge to delete");
    }
    directed_ewgts.erase(it);
}

void Graph::add_dangling(uint64_t t) {
    dangling.insert(nid_lut.get(t));
}

void Graph::print_stats() {
    uint64_t max_weight = 0;
    for (auto& [e, w] : undirected_ewgts) {
        max_weight = std::max(max_weight, w);
    }
    std::cout << "undirected:\n";
    std::cout << "\t#nodes=" << nid_lut.current_node_id << "\n";
    std::cout << "\t#edges=" << undirected_ewgts.size() << "\n";
    std::cout << "\tmax_weight=" << max_weight << "\n";
}

void Graph::clear() {
    nid_lut.clear();
    undirected_ewgts.clear();
    directed_ewgts.clear();
    dangling.clear();
}

} // namespace declustered_layout