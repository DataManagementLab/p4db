#include "graph_maxcut.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <queue>
#include <stdexcept>
#include <vector>


// #include "heuristics/maxcut/burer2002.h"
// #include "heuristics/maxcut/deSousa2013.h"


namespace declustered_layout {


Partitioning GraphMaxCut::part(Graph& graph, const Algo algo, uint32_t npart) {
    if (npart & (npart - 1)) { // not power of 2
        throw std::invalid_argument("npart is not power of 2");
    }

    auto partition = [&algo](Graph& g) {
        switch (algo) {
            case Algo::RMAXCUT:
                return part_rmaxcut(g);
            case Algo::MQLIB:
                return part_mqlib(g);
        }
        throw std::invalid_argument("invalid algorithm supplied");
    };

    struct Item {
        Graph g;
        uint32_t depth;
    };

    std::queue<Item> graph_queue;
    uint32_t depth = static_cast<uint32_t>(std::log2(npart)) - 1;
    graph_queue.emplace(Item{graph, depth});

    std::vector<Partitioning> parts;
    parts.reserve(npart);

    while (!graph_queue.empty()) {
        auto& [g, depth] = graph_queue.front();

        auto part = partition(g);
        if (depth == 0) {
            parts.emplace_back(part);
        } else {
            auto [g1, g2] = split(g, part);
            // std::cout << "g: " << g.nid_lut.current_node_id
            //           << " g1: " << g1.nid_lut.current_node_id
            //           << " g2: " << g2.nid_lut.current_node_id
            //           << " d1: " << g1.dangling.size()
            //           << " d2: " << g2.dangling.size() << '\n';
            graph_queue.emplace(Item{g1, depth - 1});
            graph_queue.emplace(Item{g2, depth - 1});
        }
        graph_queue.pop();
    }

    auto part = parts[0];
    for (std::size_t i = 1; i < parts.size(); ++i) {
        part += parts[i];
    }
    if (part.parts != npart) {
        throw std::runtime_error("partitioning failed");
    }
    return part;
}

std::tuple<Graph, Graph> GraphMaxCut::split(Graph& g, Partitioning& part) {
    Graph g1, g2;

    uint64_t cut_edges = 0;
    std::unordered_set<uint64_t> cuts[2];
    std::unordered_set<uint64_t> n1, n2;

    for (auto& [e, w] : g.undirected_ewgts) {
        auto u = g.nid_lut.rev(e.u);
        auto v = g.nid_lut.rev(e.v);
        auto p1 = part.get(u);
        auto p2 = part.get(v);
        if (p1 != p2) {
            cuts[p1].emplace(u);
            cuts[p2].emplace(v);
            ++cut_edges;
            continue; // partition crossing edge, ignore
        }
        // std::cout << "u=" << u << " v=" << v << " p1=" << p1 << " p2=" <<
        // p2 << '\n';
        if (p1 == 0) {
            g1.add_undirected_edge(u, v, w);
            n1.emplace(u);
            n1.emplace(v);
        } else {
            g2.add_undirected_edge(u, v, w);
            n2.emplace(u);
            n2.emplace(v);
        }
    }
    // std::cout << "cut_edges=" << cut_edges << '\n';

    for (auto& tid : cuts[0]) {
        if (n1.contains(tid)) {
            continue;
        }
        g1.add_dangling(tid);
    }
    for (auto& tid : cuts[1]) {
        if (n2.contains(tid)) {
            continue;
        }
        g2.add_dangling(tid);
    }

    // add dangling uniformly between both partitions
    for (size_t i = 0; auto& nid : g.dangling) {
        if ((i & 0x01) == 0) {
            g1.add_dangling(g.nid_lut.rev(nid));
        } else {
            g2.add_dangling(g.nid_lut.rev(nid));
        }
        ++i;
    }

    return std::make_tuple(g1, g2);
}

Partitioning GraphMaxCut::part_rmaxcut(Graph& g) {
    std::ofstream file;
    file.open("infile.txt");
    for (auto& [e, w] : g.undirected_ewgts) {
        file << g.nid_lut.rev(e.u) << '\t' << g.nid_lut.rev(e.v) << '\t' << w
             << '\n';
    }
    file.close();

    int rc = std::system("gzip -c infile.txt > infile.txt.gz");
    if (rc != 0) {
        throw std::runtime_error("gzip failed");
    }

    rc = std::system("./rmaxcut/rmaxcut -s 0 -r 20000 -f 0.1 infile.txt.gz > part.txt 2> /dev/null");
    if (rc != 0) {
        throw std::runtime_error("rmaxcut failed");
    }

    Partitioning part{2};

    std::ifstream infile("part.txt");
    std::string line;
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        char type;
        uint64_t node_id;
        int spin;
        if (!(iss >> type >> node_id >> spin)) {
            break;
        }
        if (type != 'N') {
            continue;
        }
        part.insert(node_id, (spin == -1)); // -1 is 0 partition
    }
    infile.close();

    // add dangling uniformly between both partitions
    for (size_t i = 0; auto& nid : g.dangling) {
        part.insert(g.nid_lut.rev(nid), (i & 0x01));
        ++i;
    }

    rc = std::system("rm infile.txt infile.txt.gz part.txt");
    if (rc != 0) {
        throw std::runtime_error("rm failed");
    }

    return part;
}

Partitioning GraphMaxCut::part_mqlib(Graph& g) {
    std::ofstream file;
    file.open("input.txt");
    file << g.nid_lut.current_node_id << " " << g.undirected_ewgts.size() << "\n";
    for (auto& [e, w] : g.undirected_ewgts) {
        // stupidly 1-indexed
        file << e.u + 1 << " " << e.v + 1 << " " << w << "\n";
    }
    file.close();

    int rc = std::system("./MQLib/bin/MQLib -h DESOUSA2013 -fM ./input.txt -r 1 -ps > output.txt"); // -r 10
    if (rc != 0) {
        throw std::runtime_error("mqlib failed");
    }

    Partitioning part{2};

    std::ifstream infile("output.txt");
    std::string line;
    while (std::getline(infile, line)) {
        if (line.starts_with("Solution:")) {
            break;
        }
    }
    uint64_t node_id = 0;
    int spin;
    while (infile >> spin) {
        part.insert(g.nid_lut.rev(node_id), (spin == -1)); // -1 is 0 partition
        ++node_id;
    }
    infile.close();


    // add dangling uniformly between both partitions
    for (size_t i = 0; auto& nid : g.dangling) {
        part.insert(g.nid_lut.rev(nid), (i & 0x01));
        ++i;
    }

    rc = std::system("rm input.txt output.txt");
    if (rc != 0) {
        throw std::runtime_error("rm failed");
    }


    return part;


    // std::vector<Instance::InstanceTuple> edgeList;
    // edgeList.reserve(g.undirected_ewgts.size());
    // for (auto& [e, w] : g.undirected_ewgts) {                               // only one-way
    //     edgeList.push_back(Instance::InstanceTuple{{e.u + 1, e.v + 1}, w}); // stupidly 1-indexed
    // }
    // int dimension = g.nid_lut.current_node_id;

    // MaxCutInstance mi(edgeList, dimension);
    // // Burer2002 heur(mi, 10.0, false, nullptr);
    // deSousa2013 heur(mi, 10.0, false, nullptr);
    // const auto& mcSol = heur.get_best_solution();
    // // std::cout << "Best objective value: " << mcSol.get_weight() << "\n";

    // Partitioning part{2};
    // const auto& solution = mcSol.get_assignments();
    // for (size_t node_id = 1; auto& sol : solution) {
    //     auto tuple_id = g.nid_lut.rev(node_id - 1);
    //     part.insert(tuple_id, (sol == -1)); // -1 is 0 partition
    //     ++node_id;
    // }
    // return part;
}


} // namespace declustered_layout