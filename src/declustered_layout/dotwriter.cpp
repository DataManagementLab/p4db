#include "dotwriter.hpp"

#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>


namespace declustered_layout {


void DotWriter::Render::render() {
    std::stringstream ss;
    ss << "dot -Gnewrank=true -Tpng " << filename << " -o " << filename << ".png";
    int rc = std::system(ss.str().c_str());
    if (rc != 0) {
        throw std::runtime_error("DotWriter.Render failed");
    }
}

DotWriter::Render DotWriter::write(std::string filename, Graph& g, Partitioning& part) {
    std::ofstream file;
    file.open(filename);
    file << "strict graph {\n";

    std::set<uint64_t> nodes;
    for (auto& [e, w] : g.undirected_ewgts) {
        auto u = g.nid_lut.rev(e.u);
        auto v = g.nid_lut.rev(e.v);
        nodes.insert(v);
        nodes.insert(u);

        file << u << " -- " << v << " [label=\"" << w << "\",weight="
             << w << ",penwidth=" << 1 << "];\n";
    }

    for (auto& node : nodes) {
        auto pid = part.get(node);
        file << node << " [color=" << colors.at(pid % colors.size()) << "];\n";
    }

    file << "}\n";
    file.close();
    return Render{filename};
}

DotWriter::Render DotWriter::write_directed(std::string filename, Graph& g) {
    std::ofstream file;
    file.open(filename);
    file << "digraph {\n";

    for (auto& [e, w] : g.directed_ewgts) {
        auto u = g.nid_lut.rev(e.u);
        auto v = g.nid_lut.rev(e.v);

        file << u << " -> " << v << " [label=\"" << w << "\",weight=" << w
             << ",penwidth=" << 1 << "];\n";
    }

    file << "}\n";
    file.close();
    return Render{filename};
}

DotWriter::Render DotWriter::write_clustered(std::string filename, Graph& g, Partitioning& part) {
    std::ofstream file;
    file.open(filename);
    file << "graph {\n";
    // file << "splines=line;\n";

    std::set<uint64_t> nodes;
    for (auto& [e, w] : g.undirected_ewgts) {
        auto u = g.nid_lut.rev(e.u);
        auto v = g.nid_lut.rev(e.v);
        nodes.insert(v);
        nodes.insert(u);

        if (part.get(u) == part.get(v)) {
            file << u << " -- " << v << " [label=\"" << w
                 << "\",weight=" << w << ",penwidth=2.0,color=orange];\n";
            continue;
        } else {
            file << u << " -- " << v << " [label=\"" << w
                 << "\",weight=" << w
                 << ",style=dashed,color=grey,fontcolor=grey];\n";
            continue;
        }

        file << u << " -- " << v << " [label=\"" << w << "\",weight=\"" << w
             << "\",penwidth=\"" << 1 << "\"];\n";
    }

    for (size_t i = 0; i < part.parts; ++i) {
        file << "subgraph cluster_" << i << "{\n";
        file << "label=<<b>Part " << i << "</b>>\n";
        file << "color=lightgrey\n";
        file << "style=\"rounded,filled\"\n";
        for (auto& node : nodes) {
            auto pid = part.get(node);
            if (pid != i) {
                continue;
            }
            file << node << " [color=" << colors.at(pid % colors.size())
                 << "];\n";
        }
        file << "}\n\n";
    }

    file << "}\n";
    file.close();
    return Render{filename};
}


} // namespace declustered_layout