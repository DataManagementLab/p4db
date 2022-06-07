#pragma once

#include <chrono>
#include <cstdint>
#include <istream>
#include <ostream>


// enum class AccessMode : uint32_t {
//     INVALID = 0x00000000,
//     READ = 0x00000001,
//     WRITE = 0x00000002,
//     READ_GRANTED = 0x0000ff01,
//     WRITE_GRANTED = 0x0000ff02,
// };

struct AccessMode {
    using value_t = uint32_t;

    static constexpr value_t INVALID = 0x00000000;
    static constexpr value_t READ = 0x00000001;
    static constexpr value_t WRITE = 0x00000002;

    value_t value;

    constexpr AccessMode() : value(AccessMode::INVALID) {}

    constexpr AccessMode(value_t value) : value(value) {}

    operator value_t() const {
        return get_clean();
    }

    bool operator==(const value_t& rhs) const {
        return value == rhs;
    }

    // bool operator==(const AccessMode& rhs) const {
    //     return value == rhs.value;
    // }

    value_t get_clean() const {
        return value & 0x000000ff;
    }

    bool by_switch() const {
        return (value >> 8) & 0xff;
    }

    void set_switch_index(uint16_t idx) {
        value |= static_cast<uint32_t>(__builtin_bswap16(idx)) << 16;
        value |= 0x0000aa00;
    }
};


struct datetime_t {
    uint64_t value;

    static datetime_t now() {
        const auto p1 = std::chrono::system_clock::now();
        using duration = std::chrono::duration<uint64_t, std::micro>;
        const auto ts = std::chrono::duration_cast<duration>(p1.time_since_epoch()).count();
        return datetime_t{ts};
    }

    operator uint64_t() const {
        return value;
    }
};


namespace p4db {


struct table_t {
    uint64_t value;
    operator uint64_t() const {
        return value;
    }
};
static_assert(std::is_trivial<table_t>::value, "table_t is not a POD");

struct key_t {
    uint64_t value;
    operator uint64_t() const {
        return value;
    }
};
static_assert(std::is_trivial<key_t>::value, "key_t is not a POD");


} // namespace p4db


struct timestamp_t {
    uint64_t value;
    operator uint64_t() const {
        return value;
    }
};


enum class BenchmarkType {
    YCSB,
    SMALLBANK,
    TPCC,
    MICRO_RECIRC,
};
inline std::istream& operator>>(std::istream& is, BenchmarkType& type) {
    std::string s;
    is >> s;
    if (s == "ycsb") {
        type = BenchmarkType::YCSB;
    } else if (s == "smallbank") {
        type = BenchmarkType::SMALLBANK;
    } else if (s == "tpcc") {
        type = BenchmarkType::TPCC;
    } else if (s == "micro_recirc") {
        type = BenchmarkType::MICRO_RECIRC;
    } else {
        throw std::invalid_argument("Could not parse BenchmarkType.");
    }
    return is;
}


enum class CC_Scheme {
    NO_WAIT,
    WAIT_DIE,
    NONE,
};
inline std::ostream& operator<<(std::ostream& os, const CC_Scheme& scheme) {
    switch (scheme) {
        case CC_Scheme::NO_WAIT:
            os << "no_wait";
            break;
        case CC_Scheme::WAIT_DIE:
            os << "wait_die";
            break;
        case CC_Scheme::NONE:
            os << "none";
            break;
    }
    return os;
}