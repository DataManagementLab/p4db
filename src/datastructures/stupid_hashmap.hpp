#pragma once

#include <array>
#include <atomic>


template <typename K, typename V, std::size_t N>
struct StupidHashMap {
    static_assert(std::is_pointer<V>::value, "T not pointer type");
    static_assert(N && ((N & (N - 1)) == 0), "N not power of 2");

    ~StupidHashMap() {
        print(); // p db.msg_handler->open_futures.print()
    }

    struct Entry {
        K k;
        std::atomic<V> v{nullptr};
    };

    struct Bucket {
        static constexpr auto NUM_SLOTS = 64 / sizeof(Entry); // one cacheline
        std::array<Entry, NUM_SLOTS> slots{};
    };

    std::array<Bucket, N> ht{};

    void insert(K key, V val) {
        auto& bucket = ht[key % N];
        for (auto& slot : bucket.slots) {
            V expected = nullptr;
            if (slot.v.compare_exchange_strong(expected, val, std::memory_order_relaxed, std::memory_order_relaxed)) {
                slot.k = key;
                return;
            }
        }
        throw std::runtime_error("Bucket full.");
    }

    auto erase(K key) {
        auto& bucket = ht[key % N];
        for (auto& slot : bucket.slots) {
            if (slot.k == key) {
                return slot.v.exchange(nullptr, std::memory_order_release);
            }
        }
        throw std::runtime_error("Key not found.");
    }

    void print() {
        for (std::size_t i = 0; i < N; ++i) {
            auto& bucket = ht[i];
            for (auto& slot : bucket.slots) {
                if (slot.v) {
                    std::cout << "k=" << slot.k << " v=" << slot.v << '\n';
                }
            }
        }
    }
};