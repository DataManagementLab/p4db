#include "counter.hpp"

#include "collector.hpp"
#include "stats/context.hpp"

#include <sstream>

namespace stats {

Counter::Counter() {
    if constexpr (!(ENABLED_STATS & StatsBitmask::COUNTER)) {
        return;
    }
    StatsCollector::get().reg(this);
}


Counter::~Counter() {
    if constexpr (!(ENABLED_STATS & StatsBitmask::COUNTER)) {
        return;
    }
    StatsCollector::get().dereg(this);

    if constexpr (STATS_PER_WORKER) {
        std::stringstream ss;
        ss << "*** Stats Worker " << WorkerContext::get().tid << " ***\n";
        for (size_t i = 0; auto& name : enum2str) {
            ss << name << '=' << counters[i++] << '\n';
        }
        std::cout << ss.str();
    }
}

} // namespace stats