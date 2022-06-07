#pragma once

#include "types.hpp"

#include <chrono>
#include <iostream>
#include <sstream>


struct ClockTimestampFactory {
    using clock = std::chrono::high_resolution_clock;

    clock::time_point start = clock::now();

    ClockTimestampFactory() {
        // std::stringstream ss;
        // ss << "start_ts=" << get() << '\n';
        // std::cout << ss.str();
    }

    timestamp_t get() {
        uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start).count();
        return timestamp_t{ts};
    }
};

struct UniqueClockTimestampFactory {
    using clock = std::chrono::high_resolution_clock;

    clock::time_point start = clock::now();
    uint64_t mask;

    UniqueClockTimestampFactory() {
        auto& config = Config::instance();
        mask = (config.node_id << 8) | WorkerContext::get().tid;

        // std::stringstream ss;
        // ss << "start_ts=" << get() << " mask: " << mask << '\n';
        // std::cout << ss.str();
    }

    timestamp_t get() {
        uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start).count();
        return timestamp_t{(ts << 16) | mask}; // 2^48 ns -> 3.25781223 days
    }
};


struct AtomicTimestampFactory {
    static inline std::atomic<uint64_t> cntr{1};

    timestamp_t get() {
        uint64_t ts = cntr.fetch_add(1);
        return timestamp_t{ts};
    }
};


// using TimestampFactory = AtomicTimestampFactory;
// using TimestampFactory = ClockTimestampFactory;
using TimestampFactory = UniqueClockTimestampFactory;