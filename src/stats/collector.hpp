#pragma once

#include "counter.hpp"
#include "cycles.hpp"
#include "moving_avg.hpp"
#include "periodic.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>


namespace stats {

struct StatsCollector {
    std::mutex mutex;
    std::jthread thread;

    StatsCollector();
    ~StatsCollector();

    static StatsCollector& get();

    std::vector<Counter*> cntrs;
    std::array<uint64_t, Counter::__MAX> sum_counters{};
    void reg(Counter* cntr);
    void dereg(Counter* cntr);

    template <typename T>
    struct TotalAverage {
        void add(T sample) {
            total += sample;
            ++n;
        }
        double avg() const {
            if (n == 0) {
                return 0.0;
            }
            return static_cast<double>(total) / n;
        }

    private:
        T total = 0;
        uint64_t n = 0;
    };

    struct CyclesInfo {
        using avg_t = MovingAverage<uint64_t, uint64_t, 1024>;
        // using avg_t = TotalAverage<uint64_t>;
        Cycles* cycles;
        std::array<avg_t, Cycles::__MAX> avgs{};
    };
    std::vector<CyclesInfo> cycles;
    std::array<TotalAverage<double>, Cycles::__MAX> avg_cycles{};

    void reg(Cycles* c);
    void dereg(Cycles* c);

    std::vector<Periodic*> pcntrs;
    std::array<uint64_t, Periodic::__MAX> last_pcntrs{};
    void reg(Periodic* cntr);
    void dereg(Periodic* cntr);
};

} // namespace stats
