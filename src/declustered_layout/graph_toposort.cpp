#include "graph_toposort.hpp"

#include "dotwriter.hpp"

#include <queue>
#include <string>
#include <string_view>


namespace declustered_layout {


void GraphTopoSort::setup(Graph& g, Partitioning& part) {
    auto dir_cut_weight = [&](const auto& pid1, const auto& pid2) {
        int64_t cross_weight = 0;

        for (auto& [e, w] : g.directed_ewgts) {
            auto u = g.nid_lut.rev(e.u);
            auto v = g.nid_lut.rev(e.v);
            auto p1 = part.get(u);
            auto p2 = part.get(v);
            if (p1 == pid1 && p2 == pid2) {
                cross_weight += w;
            } else if (p1 == pid2 && p2 == pid1) {
                cross_weight -= w;
            } else {
                // edge is within a partition
            }
        }
        return cross_weight;
    };


    nnodes = part.parts;
    for (size_t i = 0; i < part.parts; i++) {
        for (size_t j = i + 1; j < part.parts; j++) {
            auto weight = dir_cut_weight(i, j);
            if (weight == 0) {
                continue;
            }
            // std::cout << "dir_cut_weight(" << i << "," << j << ")=" << weight << "\n";
            if (weight < 0) {
                topo.add_directed_edge(j, i, -weight);
            } else {
                topo.add_directed_edge(i, j, weight);
            }
        }
    }
}

std::vector<uint64_t> GraphTopoSort::topo_sort(const Algo algo) {
    switch (algo) {
        case Algo::TIGHT:
            return tight();
        case Algo::FAS:
            return fas();
        case Algo::RANDOM: {
            std::vector<uint64_t> ordering(nnodes); //topo.nid_lut.current_node_id
            std::iota(ordering.begin(), ordering.end(), 0);
            return ordering;
        }
    }
    throw std::invalid_argument("invalid algorithm supplied");
}

std::vector<uint64_t> GraphTopoSort::tight() {
    std::ofstream file;
    file.open("graph.def");
    for (auto& [e, w] : topo.directed_ewgts) {
        file << topo.nid_lut.rev(e.u) << " " << topo.nid_lut.rev(e.v) << " " << w << '\n';
    }
    file.close();

    int rc = std::system("./FaspHeuristic/build/tools/tightCutWeighted < graph.def > output.txt"); // for max-weight
    if (rc != 0) {
        throw std::runtime_error("tight failed");
    }

    std::ifstream infile("output.txt");
    std::string line;
    while (std::getline(infile, line)) {
        if (line[0] == '#') {
            continue;
        }
        std::istringstream iss(line);
        uint64_t u;
        uint64_t v;
        if (!(iss >> u >> v)) {
            break;
        }

        auto edge = Edge::directed(topo.nid_lut.get(u), topo.nid_lut.get(v));
        topo.remove_directed_edge(edge);
    }
    infile.close();


    rc = std::system("rm graph.def output.txt");
    if (rc != 0) {
        throw std::runtime_error("rm failed");
    }

    return tsort();
}

std::vector<uint64_t> GraphTopoSort::fas() {
    std::ofstream file;
    file.open("topo.data");
    file << topo.nid_lut.current_node_id << '\n';
    for (auto& [e, w] : topo.directed_ewgts) {
        file << topo.nid_lut.rev(e.u) << " " << topo.nid_lut.rev(e.v) << " " << w << '\n';
    }
    file.close();

    int rc = std::system("./Feedback-Arc-Set/fas topo.data > output.txt"); // for max-weight
    if (rc != 0) {
        throw std::runtime_error("fas failed");
    }

    std::vector<uint64_t> top_order;
    top_order.reserve(topo.nid_lut.current_node_id);


    std::ifstream infile("output.txt");
    std::string line;
    while (std::getline(infile, line)) {
        constexpr std::string_view prefix{"Optimal ordering: "};
        if (!line.starts_with(prefix)) {
            continue;
        }

        if (line.find("||") != std::string::npos) {
            throw std::runtime_error("condorcet partition");
        }

        std::string s = line.substr(prefix.size());
        auto end = std::remove_if(s.begin(), s.end(), [](const char c) {
            return c != ' ' && !std::isdigit(c);
        });
        std::string all_numbers(s.begin(), end);

        std::istringstream iss(all_numbers);
        uint64_t num;
        for (size_t i = 0; i < topo.nid_lut.current_node_id; ++i) {
            iss >> num;
            top_order.emplace_back(num);
        }
    }
    infile.close();

    auto valid = [&](const uint64_t u, const uint64_t v) {
        bool found_first = false;
        for (auto& n : top_order) {
            if (n == u) {
                found_first = true;
            } else if (n == v) {
                return found_first;
            }
        }
        return false;
    };

    for (auto it = topo.directed_ewgts.begin(); it != topo.directed_ewgts.end();) {
        auto& e = it->first;
        if (!valid(topo.nid_lut.rev(e.u), topo.nid_lut.rev(e.v))) {
            it = topo.directed_ewgts.erase(it); // or "it = m.erase(it)" since C++11
        } else {
            ++it;
        }
    }

    rc = std::system("rm topo.data"); // output.txt");
    if (rc != 0) {
        throw std::runtime_error("rm failed");
    }

    std::cout << "top_order=" << top_order << '\n';
    std::vector<uint64_t> ordering(top_order.size());
    for (size_t i = 0; i < top_order.size(); ++i) {
        ordering[top_order[i]] = i;
    } // defines which partition should map to which
    return ordering;
}

std::vector<uint64_t> GraphTopoSort::tsort() {
    // topology graph
    adj_lut.clear();
    adj_lut.resize(topo.nid_lut.current_node_id);
    for (auto& [e, w] : topo.directed_ewgts) {
        adj_lut[e.u].emplace_back(e.v);
    }

    // if (has_cycle()) {
    //     throw std::runtime_error("topology has cycle?");
    // }

    const size_t N = adj_lut.size();
    std::vector<uint64_t> in_degree(N);

    for (auto& adj : adj_lut) {
        for (auto& u : adj) {
            ++in_degree[u];
        }
    }

    std::queue<uint64_t> q;
    for (uint64_t i = 0; i < N; ++i) {
        if (in_degree[i] == 0) {
            q.push(i);
        }
    }

    uint64_t cnt = 0;
    std::vector<uint64_t> top_order;
    top_order.reserve(N);

    while (!q.empty()) {
        uint64_t u = q.front();
        q.pop();

        top_order.emplace_back(u);

        for (auto& v : adj_lut[u]) {
            if (--in_degree[v] == 0) {
                q.push(v);
            }
        }
        cnt++;
    }

    if (cnt != N) {
        throw std::runtime_error("there exists cycle in graph");
    }

    for (auto& n : top_order) {
        n = topo.nid_lut.rev(n);
    }
    std::cout << "top_order=" << top_order << '\n';

    std::vector<uint64_t> ordering(top_order.size());
    for (size_t i = 0; i < top_order.size(); ++i) {
        ordering[top_order[i]] = i;
    } // defines which partition should map to which
    return ordering;
}

bool GraphTopoSort::has_cycle() {
    const size_t N = adj_lut.size();

    std::vector<bool> visited(N);
    std::vector<bool> recStack(N);
    for (size_t i = 0; i < N; ++i) {
        if (has_cycle_until(i, visited, recStack)) {
            return true;
        }
    }
    return false;
}

bool GraphTopoSort::has_cycle_until(uint64_t v, std::vector<bool>& visited, std::vector<bool>& recStack) {
    if (visited[v] == false) {
        // Mark the current node as visited and part of recursion stack
        visited[v] = true;
        recStack[v] = true;

        // Recur for all the vertices adjacent to this vertex
        for (auto& u : adj_lut[v]) {
            if (!visited[u] && has_cycle_until(u, visited, recStack))
                return true;
            else if (recStack[u])
                return true;
        }
    }
    recStack[v] = false; // remove the vertex from recursion stack
    return false;
}


} // namespace declustered_layout