#include "util.hpp"

#include "stats/context.hpp"

#include <numeric>


void pin_worker(uint32_t core, pthread_t pid /*= pthread_self()*/) {
    WorkerContext::get().tid = core;
    core += 2; // make space for dpdk main and receiver thread

    constexpr auto NUM_SOCKETS = 2;
    constexpr auto NUM_HYPERTHREADS = 2;
    static const auto cpu_map = []() {
        std::vector<int> map;

        // socket 0: real-cores , hyper threads, then socket 1:
        auto threads = std::thread::hardware_concurrency();
        if (threads == 0) {
            throw std::runtime_error("std::thread::hardware_concurrency() failed.");
        }
        map.reserve(threads);

        auto per_socket = threads / NUM_SOCKETS;
        auto real_cores = per_socket / NUM_HYPERTHREADS;
        for (auto socket = 0; socket < NUM_SOCKETS; ++socket) {
            for (auto i = socket * real_cores; i < (socket + 1) * real_cores; ++i) {
                map.emplace_back(i);
            }
            for (auto i = (NUM_SOCKETS + socket) * real_cores; i < (NUM_SOCKETS + socket + 1) * real_cores; ++i) {
                map.emplace_back(i);
            }
        }

        if constexpr (SINGLE_NUMA) {
            map.resize(map.size() / 2);
        }

        std::stringstream ss;
        ss << "CPU MAP:\n";
        for (auto& c : map) {
            ss << c << ' ';
        }
        ss << '\n';
        std::cout << ss.str();

        return map;
    }();


    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_map.at(core % cpu_map.size()), &mask);

    if (core >= cpu_map.size()) {
        std::cout << "WARNING more than one pinned thread per core!\n";
    }

    // (void) pid;
    if (pthread_setaffinity_np(pid, sizeof(cpu_set_t), &mask) != 0) {
        std::perror("pthread_setaffinity_np");
    }
}


// formatting of bytes

std::string stringifyFraction(const uint64_t num, const unsigned den, const unsigned precision) {
    constexpr unsigned base = 10;

    // prevent division by zero if necessary
    if (den == 0) {
        return "inf";
    }

    // integral part can be computed using regular division
    std::string result = std::to_string(num / den);

    // perform first step of long division
    // also cancel early if there is no fractional part
    unsigned tmp = num % den;
    if (tmp == 0 || precision == 0) {
        return result;
    }

    // reserve characters to avoid unnecessary re-allocation
    result.reserve(result.size() + precision + 1);

    // fractional part can be computed using long divison
    result += '.';
    for (size_t i = 0; i < precision; ++i) {
        tmp *= base;
        char nextDigit = '0' + static_cast<char>(tmp / den);
        result.push_back(nextDigit);
        tmp %= den;
    }

    return result;
}
