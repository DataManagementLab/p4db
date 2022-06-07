#pragma once


#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>


template <std::size_t N>
struct Scheduler {
    struct Entry {
        std::function<void()> function;
        std::chrono::microseconds interval;
        std::chrono::time_point<std::chrono::steady_clock> next{};

        bool operator<(const Entry& other) {
            return next < other.next;
        }
    };

    std::array<Entry, N> entries;

    Scheduler(std::array<Entry, N>&& entries)
        : entries(std::move(entries)) {}

    void run_next() {
        auto now = std::chrono::steady_clock::now();
        auto e = std::min_element(entries.begin(), entries.end());
        if (e->next > now) {
            std::this_thread::sleep_until(e->next);
        }

        now = std::chrono::steady_clock::now();
        e->function();
        e->next = now + e->interval;
    }
};
