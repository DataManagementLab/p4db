#include "declustered_layout.hpp"
#include "switch_simulator.hpp"
#include "transaction.hpp"


int main() {
    using namespace declustered_layout;


    DeclusteredLayout dcl;
    std::vector<Transaction> txns;

    Transaction t1;
    t1.generate_n(8);
    t1.rerun(5);
    t1.generate_chain_dep();
    // t1.dependency(t1.accesses.back(), t1.accesses.front());
    dcl.add_sample(t1);
    txns.emplace_back(t1);


    // Transaction t2;
    // t2.generate_n(8);
    // t2.rerun(5);
    // dcl.add_sample(t2);
    // txns.emplace_back(t2);

    Transaction t5;
    t5.generate_n(8);
    t5.rerun(5);
    t5.generate_chain_dep();
    dcl.add_sample(t5);
    txns.emplace_back(t5);

    // add some cross-partition
    Transaction t6;
    for (auto& a : t1.accesses) {
        t6.access(a);
    }
    t6.access(t5.accesses[0]);
    t6.access(t5.accesses[1]);
    dcl.add_sample(t6);
    txns.emplace_back(t6);

    // for (int i = 0; i < 100000; ++i) {
    //     Transaction t;
    //     t.generate_n(8);
    //     t.rerun(rand() % 100 + 1);
    //     dcl.add_sample(t);
    //     txns.emplace_back(t);
    // }

    dcl.compute_layout(true, true);
    dcl.print();


    SwitchSimulator sim{dcl};
    sim.include_deps = true;
    sim.process(txns);


    return 0;
}