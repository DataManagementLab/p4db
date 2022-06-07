#include "transaction.hpp"


namespace declustered_layout {


void Transaction::generate_n(std::size_t n) {
    accesses.reserve(accesses.size() + n);
    for (size_t i = 0; i < n; ++i) {
        auto access = gen();
        if (std::find(accesses.begin(), accesses.end(), access) != accesses.end()) {
            --i;
            continue; // prevent duplicates
        }
        accesses.emplace_back(access);
    }
}

void Transaction::generate_chain_dep() {
    if (accesses.size() <= 2) {
        throw std::runtime_error("transaction must at least have 2 accesses");
    }
    for (size_t i = 0; i < accesses.size() - 1; ++i) {
        dependency(accesses[i], accesses[i + 1]);
    }
}

void Transaction::dependency(uint64_t tid1, uint64_t tid2) {
    deps.emplace_back(tid1, tid2);
}

void Transaction::rerun(uint64_t times) {
    repeats = times;
};

void Transaction::access(uint64_t tid) {
    accesses.emplace_back(tid);
}


} // namespace declustered_layout