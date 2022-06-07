#pragma once

#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>


struct Utils {
    template <typename T>
    static std::string to_hex(T i) {
        std::stringstream stream;
        stream << "0x"
               << std::setfill('0') << std::setw(sizeof(T) * 2)
               << std::hex << i;
        return stream.str();
    }
};


#define EXIT_WITH_ERROR(reason, ...)                                \
    do {                                                            \
        printf("Terminated in error: " reason "\n", ##__VA_ARGS__); \
        exit(1);                                                    \
    } while (0)


struct CPUSet {
    std::set<uint32_t> available;
    std::set<uint32_t> in_use;

    CPUSet() = default;

    CPUSet(std::initializer_list<uint32_t> cpus) : available(cpus) {
        if (available.size() != cpus.size()) {
            EXIT_WITH_ERROR("Elements in CPUSet not unique");
        }
    }

    bool is_available(uint32_t core) {
        return available.contains(core);
    }

    void mark_in_use(uint32_t core) {
        size_t r = available.erase(core);
        if (r != 1) {
            EXIT_WITH_ERROR("mark_in_use(): Core %d not found", core);
        }
        in_use.insert(core);
    }

    void mark_unused(uint32_t core) {
        size_t r = in_use.erase(core);
        if (r != 1) {
            EXIT_WITH_ERROR("mark_unused(): Core %d not found", core);
        }
        available.insert(core);
    }

    uint32_t get_free_core() {
        auto it = available.begin();
        if (it == available.end()) {
            EXIT_WITH_ERROR("No cores available");
        }
        uint32_t core = *it;
        available.erase(it);
        in_use.insert(core);
        return core;
    }

    std::string hex_str() const {
        uint32_t max_core = *available.rbegin();
        if (max_core >= 32) {
            EXIT_WITH_ERROR("rte does not support more that 32 core for now");
        }
        uint32_t mask = 0;
        for (auto& core : available) {
            mask |= (1UL << (32 - core));
        }
        return Utils::to_hex(mask);
    }

    std::string join(std::string delimiter) {
        std::stringstream stream;
        for (auto& core : available) {
            stream << core;
            if (core != *available.rbegin()) {
                stream << delimiter;
            }
        }
        return stream.str();
    }
};