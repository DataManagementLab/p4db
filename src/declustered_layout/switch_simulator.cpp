#include "switch_simulator.hpp"


namespace declustered_layout {


SwitchSimulator::SwitchSimulator(DeclusteredLayout& dcl) : dcl(dcl) {
    // if (part.parts > NUM_REGS) {
    //     throw std::invalid_argument("partitioning contains more partitions "
    //                                 "than available registers");
    // }
}

void SwitchSimulator::process(std::vector<Transaction> txns) {
    std::vector<uint64_t> pass_hist(DeclusteredLayout::MAX_ACCESSES);
    std::vector<uint64_t> reg_hist(DeclusteredLayout::STAGES);
    std::vector<uint64_t> regs(DeclusteredLayout::MAX_ACCESSES); // trail of accesses
    uint64_t total_txns = 0;                                     // inclusive repeats

    for (size_t i = 0; auto& txn : txns) {
        if (!(txn.accesses.size() <= DeclusteredLayout::MAX_ACCESSES)) {
            std::cout << "txn.accesses.size()=" << txn.accesses.size() << '\n';
            throw std::invalid_argument("transaction needs more accesses than allowed");
        }

        regs.clear();
        std::fill(reg_hist.begin(), reg_hist.end(), 0);
        for (auto& access : txn.accesses) {
            uint64_t reg = dcl.get_location(access).stage_id;
            reg_hist[reg] += 1;
            regs.emplace_back(reg);
        }

        uint64_t passes = *std::max_element(reg_hist.begin(), reg_hist.end());


        uint64_t violated_deps = 0;
        if (include_deps) {
            for (auto& dep : txn.deps) {
                auto p1 = dcl.get_location(dep.tid1).stage_id;
                auto p2 = dcl.get_location(dep.tid2).stage_id;
                if (p1 >= p2) {
                    // std::cout << "dep violated: " << p1 << "<" << p2 << '\n';
                    ++violated_deps;
                }
            }
        }

        pass_hist[passes - 1] += txn.repeats;
        total_txns += txn.repeats;
        std::cout << "txn[" << (i++) << "] passes=" << passes << " !deps=" << violated_deps
                  << " --> " << txn.accesses << " regs=" << regs << "\n";
    }

    // crop last zero elements
    auto it = std::find_if(pass_hist.rbegin(), pass_hist.rend(), [](auto x) {
        return x > 0;
    });
    auto max_passes = std::distance(it, pass_hist.rend());
    pass_hist.resize(max_passes);

    std::vector<double> pass_dist;
    pass_dist.reserve(pass_hist.size());
    for (auto& x : pass_hist) {
        pass_dist.emplace_back(static_cast<double>(x) / total_txns);
    }

    // print results
    std::cout << "pass_hist={single,multi,...}=" << pass_hist << "\n";
    std::streamsize ss = std::cout.precision();
    std::cout.precision(4);
    std::cout << "pass_dist={single,multi,...}=" << pass_dist << "\n";
    std::cout.precision(ss);
}


} // namespace declustered_layout