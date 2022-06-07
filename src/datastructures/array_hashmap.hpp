#pragma once


#include <array>
#include <cstdint>
#include <utility>


template <typename K, typename V, std::size_t N>
struct ArrayHashMap {
    static_assert(std::is_pointer<V>::value, "T not pointer type");
    static_assert(N && ((N & (N - 1)) == 0), "N not power of 2");

    ~ArrayHashMap() {
        print();
    }

    std::array<V, N> ht{};

    void insert(K key, V val) {
        auto& entry = ht[key % N];
        if (entry) {
            throw std::runtime_error("Entry not empty");
        }
        entry = val;
    }

    auto erase(K key) {
        auto& entry = ht[key % N];
        if (!entry) {
            throw std::runtime_error("Entry not set");
        }
        return std::exchange(entry, nullptr);
    }

    void print() {
        for (std::size_t i = 0; i < N; ++i) {
            auto& entry = ht[i];
            if (entry) {
                std::cout << "i=" << i << " v=" << entry << '\n';
            }
        }
    }
};
