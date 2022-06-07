#pragma once


#include "db/defs.hpp"
#include "db/util.hpp"

#include <array>
#include <atomic>
#include <string_view>


namespace stats {


struct Periodic {
    Periodic();
    ~Periodic();

    enum Name {
        commits,
        __MAX
    };

    static constexpr std::array<std::string_view, __MAX> enum2str{
        "commits",
    };

    std::atomic<uint64_t> counters[__MAX]{};


#define __forceinline inline __attribute__((always_inline))

    __forceinline void incr(const Name name) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::PERIODIC)) {
            return;
        }
        auto& cntr = counters[name];
        auto local = cntr.load(std::memory_order_release);
        ++local;
        cntr.store(local, std::memory_order_relaxed);
    }
};

} // namespace stats
