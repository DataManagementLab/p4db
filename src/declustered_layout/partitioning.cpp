#include "partitioning.hpp"


namespace declustered_layout {


Partitioning::Partitioning(uint64_t parts) : parts(parts) {}

void Partitioning::insert(uint64_t tid, uint64_t pid) {
    if (map.contains(tid)) {
        throw std::invalid_argument("mapping already contains tid");
    }
    if (!(pid < parts)) {
        throw std::invalid_argument("pid is not within parts");
    }
    map[tid] = pid;
}

bool Partitioning::has(uint64_t tid) const {
    return map.contains(tid);
}


const uint64_t& Partitioning::get(const uint64_t tid) const {
    return map.at(tid);
}

Partitioning& Partitioning::operator+=(const Partitioning& rhs) {
    uint64_t offset = parts;
    parts += rhs.parts;
    for (auto& [tid, pid] : rhs.map) {
        insert(tid, pid + offset);
    }
    return *this;
}

void Partitioning::print() {
    struct Pair {
        uint64_t tid;
        uint64_t pid;

        bool operator<(const Pair& rhs) const { return tid < rhs.tid; }
    };
    std::vector<Pair> part_vec;
    part_vec.reserve(map.size());
    for (auto& [tid, reg] : map) {
        part_vec.emplace_back(tid, reg);
    }
    std::sort(part_vec.begin(), part_vec.end());
    for (auto& [tid, reg] : part_vec) {
        std::cout << "tuple[" << tid << "] --> " << reg << "\n";
    }
}

void Partitioning::print_stats() {
    std::vector<uint64_t> part_sizes(parts);
    for (auto& [tid, reg] : map) {
        part_sizes[reg]++;
    }
    std::cout << "#parts=" << parts << " partition_sizes=" << part_sizes
              << "\n";
}


} // namespace declustered_layout
