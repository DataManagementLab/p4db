#include "declustered_layout.hpp"

#include "dotwriter.hpp"
#include "graph_maxcut.hpp"
#include "graph_toposort.hpp"


namespace declustered_layout {


void DeclusteredLayout::add_sample(Transaction& txn) {
    const std::size_t N = txn.accesses.size();
    for (size_t i = 0; i < N; i++) {
        for (size_t j = i; j < N; j++) {
            if (i == j) {
                continue;
            }
            g.add_undirected_edge(txn.accesses[i], txn.accesses[j], txn.repeats);
        }
    }

    for (auto& dep : txn.deps) {
        g.add_directed_edge(dep.tid1, dep.tid2, txn.repeats);
    }
}

void DeclusteredLayout::compute_layout(bool topo_sort, bool write_dot) {
    g.print_stats();
    auto part = GraphMaxCut::part(g, GraphMaxCut::RMAXCUT, PARTITIONS);
    // auto part = GraphMaxCut::part(g, GraphMaxCut::MQLIB, PARTITIONS);
    part.print_stats();

    GraphTopoSort gts;
    gts.setup(g, part);
    if (write_dot) {
        DotWriter::write_directed("graph_topo.dot", gts.topo).render();
    }

    // actually only needed when we have dependencies, e.g. SmallBank
    if (topo_sort) {
        auto ordering = gts.topo_sort(GraphTopoSort::TIGHT);
        // auto ordering = gts.topo_sort(GraphTopoSort::FAS);  // some bug where topo graph has < 8 nodes
        // auto ordering = gts.topo_sort(GraphTopoSort::RANDOM);

        if (write_dot) {
            DotWriter::write_directed("graph_topo_dag.dot", gts.topo).render(); // contains DAG
        }

        // part.print();
        std::cout << "ordering=" << ordering << '\n';
        for (auto& [tid, pid] : part.map) {
            pid = ordering[pid];
        }
        // part.print();
    }

    if (write_dot) {
        DotWriter::write("graph_1.dot", g, part).render();
        DotWriter::write_clustered("graph_2.dot", g, part).render();
    }


    std::array<uint64_t, PARTITIONS> reg_fill{};
    for (auto& [tid, pid] : part.map) {
        TupleLocation tl;
        tl.stage_id = pid;
        tl.reg_array_id = 0; // unused 0-4
        tl.reg_array_idx = reg_fill[tl.stage_id * REGS_PER_STAGE + tl.reg_array_id]++;
        tl.lock_bit = 0; // unsued 0 | 1


        if (!(tl.stage_id < STAGES)) {
            throw std::runtime_error("tl.stage_id >= STAGES");
        }
        if (!(tl.reg_array_id < REGS_PER_STAGE)) {
            throw std::runtime_error("tl.reg_array_id >= REGS_PER_STAGE");
        }
        if (!(tl.reg_array_idx < REG_SIZE)) {
            std::cout << "idx=" << tl.reg_array_idx << "\n";
            throw std::runtime_error("tl.reg_array_idx >= REG_SIZE");
        }


        // We partition the register-array internally by #lock-bits using Maxcut
        // then assign each tuple within a register-array 0 or 1 as lock-bit
        // this lock-bit distribution does NOT have to be uniform.

        switch_tuples[tid] = tl;
    }
}

bool DeclusteredLayout::is_hot(uint64_t idx) const {
    return switch_tuples.find(idx) != switch_tuples.end();
}

const TupleLocation& DeclusteredLayout::get_location(const uint64_t idx) const {
    return switch_tuples.at(idx);
}

void DeclusteredLayout::clear() {
    switch_tuples.clear();
    // g.clear();
}

void DeclusteredLayout::print() {
    for (auto& [tid, tl] : switch_tuples) {
        std::cout << "tuple[" << tid << "]=" << tl << '\n';
    }
}


} // namespace declustered_layout