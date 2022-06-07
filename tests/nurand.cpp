#include "benchmarks/tpcc/random.hpp"
#include "db/defs.hpp"

#include <array>
#include <cstdint>
#include <iostream>

// g++ -std=c++20 -g -O3 -march=native -pthread -I../src nurand.cpp -o nurand && ./nurand

int main(void) {
    using namespace benchmark::tpcc;

    TPCCRandom rnd(0);
    std::array<std::pair<uint64_t, uint64_t>, NUM_ITEMS> cntr{};
    for (size_t i = 0; auto& e : cntr) {
        e.first = i++;
    }

    constexpr uint64_t TOTAL = 10'000'000;

    for (size_t i = 0; i < TOTAL; ++i) {
        auto val = rnd.NURand<uint64_t>(1023, 1, CUSTOMER_PER_DISTRICT) - 1;
        //auto val = rnd.NURand<uint64_t>(8191, 1, NUM_ITEMS)-1;
        auto& e = cntr[val];
        e.second++;
    }

    std::sort(cntr.begin(), cntr.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    for (size_t i = 0; auto& e : cntr) {
        e.first = i++;
    }

    std::cout << "index,hits,percent\n";
    uint64_t sum = 0;
    for (size_t i = 0; auto& e : cntr) {
        sum += e.second;
        std::cout << e.first << ',' << e.second << ',' << (double)sum * 100 / TOTAL << '\n';
        if (i++ < 32) {
            std::cerr << e.second << " --> " << (double)sum * 100 / TOTAL << '\n';
        }
    }

    return 0;
}