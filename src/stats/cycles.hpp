#pragma once

#include "db/defs.hpp"
#include "db/util.hpp"

#include <array>
#include <atomic>
#include <string_view>
#if defined(__x86_64__)
#include <x86intrin.h>
#endif

namespace stats {

struct Cycles {
    Cycles();
    ~Cycles();


    enum Name {
        commit_latency,
        latch_contention,
        remote_latency,
        local_latency,
        switch_txn_latency,
        __MAX
    };

    static constexpr std::array<std::string_view, __MAX> enum2str{
        "commit_latency",
        "latch_contention",
        "remote_latency",
        "local_latency",
        "switch_txn_latency",
    };
    static_assert(enum2str.size() == __MAX);

    struct Entry {
        uint64_t sum = 0;
        uint64_t start = 0;
        std::atomic<uint64_t> cycles{};
    };
    std::array<Entry, __MAX> cycles{};


#define __forceinline inline __attribute__((always_inline))


    __forceinline void start(const Name name [[maybe_unused]]) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
            return;
        }
#if defined(__x86_64__)
        auto& lat = cycles[name];
        // _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
        lat.start = __builtin_ia32_rdtsc();
        // _mm_lfence();  // optionally block later instructions until rdtsc retires
#endif
    }

    __forceinline void stop(const Name name [[maybe_unused]]) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
            return;
        }
#if defined(__x86_64__)
        auto& lat = cycles[name];
        auto cycles = __builtin_ia32_rdtsc() - lat.start;
        lat.sum += cycles;
#endif
    }

    __forceinline void save(const Name name [[maybe_unused]]) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
            return;
        }
#if defined(__x86_64__)
        auto& lat = cycles[name];
        // lat.cycles.store(lat.sum, std::memory_order_relaxed);
        lat.cycles.fetch_add(lat.sum, std::memory_order_relaxed);
#endif
    }

    __forceinline void reset(const Name name [[maybe_unused]]) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
            return;
        }
        auto& lat = cycles[name];
        lat.sum = 0;
    }
};

} // namespace stats