#include "benchmarks/benchmarks.hpp"
#include "db/config.hpp"


// #define USE_VTUNE
#ifdef USE_VTUNE
#include <ittnotify.h>
#endif

#ifdef USE_VTUNE
__itt_pause();
#endif // USE_VTUNE

#ifdef USE_VTUNE
__itt_resume();
#endif // USE_VTUNE


int main(int argc, char** argv) {
    auto& config = Config::instance();
    config.parse_cli(argc, argv);
    config.print();

    switch (config.workload) {
        case BenchmarkType::YCSB: {
            using namespace benchmark::ycsb;
            return ycsb();
        }
        case BenchmarkType::SMALLBANK: {
            using namespace benchmark::smallbank;
            return smallbank();
        }
        case BenchmarkType::TPCC: {
            using namespace benchmark::tpcc;
            return tpcc();
        }
        case BenchmarkType::MICRO_RECIRC: {
            using namespace benchmark::micro_recirc;
            return micro_recirc();
        }
    }
}