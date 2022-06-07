#include "collector.hpp"

#include "db/config.hpp"
#include "scheduler.hpp"

#include <fstream>


namespace stats {

StatsCollector::StatsCollector() {
    if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES || ENABLED_STATS & StatsBitmask::PERIODIC)) {
        return;
    }

    thread = std::jthread([&](std::stop_token token) {
        // MovingAverage<uint64_t, uint64_t, 1024> avg;
        auto& config = Config::instance();
        auto node_id = config.node_id;


        // std::ofstream csv_cycles;
        // csv_cycles.open(config.csv_file_cycles);
        // csv_cycles << "node_id,tid,name,cycles\n";

        auto collect_cycles = [&]() {
            if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
                return;
            }

            const std::lock_guard<std::mutex> lock(mutex);
            for (size_t i = 0; auto& c_info : cycles) {
                for (size_t j = 0; j < Cycles::__MAX; ++j) {
                    auto val = c_info.cycles->cycles[j].cycles.exchange(0);
                    // if (i == 8) {
                    //     std::cout << "current val: " << j << ',' << Cycles::enum2str[j] << ',' << val << '\n';
                    // }
                    if (val == 0) {
                        continue;
                    }
                    // csv_cycles << node_id << ',' << i << ',' << name << ',' << val << '\n';
                    c_info.avgs.at(j).add(val);
                }
                ++i;
            }
        };


        auto use_switch = Config::instance().use_switch;
        std::ofstream csv_periodic;
        csv_periodic.open(PERIODIC_CSV_FILENAME);
        csv_periodic << "node_id,name,value,cc_scheme,use_switch\n";
        csv_periodic << std::boolalpha;

        auto collect_periodic = [&]() {
            if constexpr (!(ENABLED_STATS & StatsBitmask::PERIODIC)) {
                return;
            }

            const std::lock_guard<std::mutex> lock(mutex);
            for (size_t i = 0; auto& name : Periodic::enum2str) {
                auto& last = last_pcntrs[i];
                uint64_t sum = 0;
                for (auto& c : pcntrs) {
                    sum += c->counters[i].load(std::memory_order_release);
                }
                if (sum > last) {
                    auto diff = sum - last;
                    diff *= 1s / STATS_PERIODIC_SAMPLE_TIME;
                    last = sum;
                    csv_periodic << node_id << ',' << name << ',' << diff << ',' << CC_SCHEME << ',' << use_switch << '\n';
                    // std::cout << node_id << ',' << name << ',' << diff << ',' << CC_SCHEME << ',' << use_switch << '\n';
                }
                ++i;
            }
        };


        Scheduler<2> sched({{{collect_cycles, STATS_CYCLE_SAMPLE_TIME},
                             {collect_periodic, STATS_PERIODIC_SAMPLE_TIME}}});


        while (!token.stop_requested()) {
            sched.run_next();
        }
    });
}


StatsCollector::~StatsCollector() {
    if constexpr (ENABLED_STATS & StatsBitmask::COUNTER) {
        std::stringstream ss;
        ss << "*** Counter Summary ***\n";
        for (size_t i = 0; auto& name : Counter::enum2str) {
            ss << name << '=' << sum_counters[i++] << '\n';
        }
        std::cout << ss.str();
    }

    if constexpr (ENABLED_STATS & StatsBitmask::CYCLES) {
        std::stringstream ss;
        ss << "*** Cycles Summary ***\n";
        for (size_t i = 0; auto& name : Cycles::enum2str) {
            ss << name << '=' << avg_cycles.at(i++).avg() << '\n';
        }
        std::cout << ss.str();
    }
}

StatsCollector& StatsCollector::get() { // static
    static StatsCollector instance;
    return instance;
}

void StatsCollector::reg(Counter* cntr) {
    const std::lock_guard<std::mutex> lock(mutex);
    cntrs.push_back(cntr);
}

void StatsCollector::dereg(Counter* cntr) {
    const std::lock_guard<std::mutex> lock(mutex);
    cntrs.erase(std::remove(cntrs.begin(), cntrs.end(), cntr), cntrs.end());

    for (size_t i = 0; i < Counter::__MAX; ++i) {
        sum_counters[i] += cntr->counters[i];
    }
}

void StatsCollector::reg(Cycles* c) {
    const std::lock_guard<std::mutex> lock(mutex);
    cycles.emplace_back(c);
}
void StatsCollector::dereg(Cycles* c) {
    const std::lock_guard<std::mutex> lock(mutex);

    auto cmp = [&](auto& cycle_info) {
        if (cycle_info.cycles != c) {
            return false;
        }

        for (size_t i = 0; i < Cycles::__MAX; ++i) {
            avg_cycles.at(i).add(cycle_info.avgs.at(i).avg());
        }
        return true;
    };
    cycles.erase(std::remove_if(cycles.begin(), cycles.end(), cmp),
                 cycles.end());
    // cycles.erase(std::remove(cycles.begin(), cycles.end(), c), cycles.end());
}


void StatsCollector::reg(Periodic* cntr) {
    const std::lock_guard<std::mutex> lock(mutex);
    pcntrs.push_back(cntr);
}

void StatsCollector::dereg(Periodic* cntr) {
    const std::lock_guard<std::mutex> lock(mutex);
    pcntrs.erase(std::remove(pcntrs.begin(), pcntrs.end(), cntr), pcntrs.end());
}

} // namespace stats