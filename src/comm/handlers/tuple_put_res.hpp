#pragma once


#include "comm/comm.hpp"

#include <vector>


struct TuplePutResHandler {
    struct Counter {
        alignas(64) std::atomic<uint64_t> cnt{};
        void incr();
        void decr();
        void wait_zero();
    };
    std::vector<Counter> counts;

    TuplePutResHandler();

    void add(size_t index);
    void handle(msg::node_t node);
    void wait(size_t index);
};
