#include "cycles.hpp"

#include "collector.hpp"

#include <sstream>


namespace stats {

Cycles::Cycles() {
    if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
        return;
    }
    StatsCollector::get().reg(this);
}

Cycles::~Cycles() {
    if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
        return;
    }
    StatsCollector::get().dereg(this);

    if constexpr (STATS_PER_WORKER) {
        std::stringstream ss;
        ss << "*** Cycles Worker ***\n";
        for (size_t i = 0; auto& name : enum2str) {
            ss << name << '=' << cycles[i++].cycles << '\n';
        }
        std::cout << ss.str();
    }
}

} // namespace stats