#pragma once

#include "defs.hpp"

#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

template <typename T>
void do_not_optimize(T const& val) {
    asm volatile(""
                 :
                 : "g"(val)
                 : "memory");
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    // std::copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ","));
    os << "{";
    for (size_t i = 0; auto& v : vec) {
        if (i++ != 0) {
            os << ",";
        }
        os << v;
    }
    os << "}";
    return os;
}

template <typename T>
struct crtp {
    T& underlying() { return static_cast<T&>(*this); }
    T const& underlying() const { return static_cast<T const&>(*this); }
};

template <typename... T>
constexpr auto make_array(T&&... values) -> std::array<
    typename std::decay<typename std::common_type<T...>::type>::type,
    sizeof...(T)> {
    return std::array<
        typename std::decay<typename std::common_type<T...>::type>::type,
        sizeof...(T)>{std::forward<T>(values)...};
}

namespace detail {
template <typename T>
constexpr void hash_combine(uint64_t& seed, T const& v) {
    // 8-bit:      0x9E
    // 16-bit:     0x9E37
    // 32-bit:     0x9E3779B9
    // 64-bit:     0x9e3779b97f4a7c15
    // 128-bit:    0x9e3779b97f4a7c15f39cc0605d396154
    seed ^= std::hash<T>{}(v) + 0x9e3779b97f4a7c15 + (seed << 6) + (seed >> 2);
}

template <typename T>
constexpr void do_hash(uint64_t& seed, T v) {
    hash_combine(seed, v);
}

template <typename T, typename... Args>
constexpr void do_hash(uint64_t& seed, T v, Args... args) {
    hash_combine(seed, v);
    do_hash(seed, args...);
}
} // namespace detail

template <typename... Args>
constexpr uint64_t multi_hash(Args... args) {
    uint64_t seed = 0;
    detail::do_hash(seed, args...);
    return seed;
}


constexpr bool contains(std::string_view const what, std::string_view const where) noexcept {
    return std::string_view::npos != where.find(what);
}


void pin_worker(uint32_t core, pthread_t pid = pthread_self());


template <typename... Args>
struct parameter_pack {
    template <template <typename...> typename T>
    using apply = T<Args...>;

    template <template <typename> typename T>
    using wrap = parameter_pack<T<Args>...>;

    template <template <typename> typename T, template <typename> typename U>
    using wrap2 = parameter_pack<T<Args>..., U<Args>...>;
};

template <typename... Args>
struct parameter_pack<std::tuple<Args...>> {
    using dive = parameter_pack<Args...>;
};
template <typename... Args>
struct parameter_pack<std::variant<Args...>> {
    using dive = parameter_pack<Args...>;
};


// #include <csetjmp>
// inline thread_local jmp_buf env;
// inline thread_local int err;
// #define try if (!(err = setjmp(env)))
// #define catch(...) else
// #define throw longjmp(env, err);

template <typename T>
class HeapSingleton {
    static inline T* _instance = nullptr;

protected:
    HeapSingleton() = default;

    ~HeapSingleton() {
        if (_instance) {
            delete _instance;
        }
    }

public:
    static T& instance() {
        if (!_instance) {
            _instance = new T{};
        }
        return *_instance;
    }
};


std::string stringifyFraction(const uint64_t num, const unsigned den, const unsigned precision);


constexpr unsigned log2floor(uint64_t x) {
    // implementation for C++17 using clang or gcc
    return x ? 63 - __builtin_clzll(x) : 0;

    // implementation using the new C++20 <bit> header
    return x ? 63 - std::countl_zero(x) : 0;
}

constexpr unsigned log10floor(unsigned x) {
    constexpr unsigned char guesses[32] = {
        0, 0, 0, 0, 1, 1, 1, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
        6, 6, 6, 6, 7, 7, 7, 8, 8, 8,
        9, 9};
    constexpr uint64_t powers[11] = {
        1, 10, 100, 1000, 10000, 100000, 1000000,
        10000000, 100000000, 1000000000, 10000000000};
    unsigned guess = guesses[log2floor(x)];
    return guess + (x >= powers[guess + 1]);
}

template <typename Uint>
constexpr Uint logFloor_naive(Uint val, unsigned base) {
    Uint result = 0;
    while (val /= base) {
        ++result;
    }
    return result;
}


template <typename Uint, size_t BASE>
constexpr std::array<uint8_t, std::numeric_limits<Uint>::digits> makeGuessTable() {
    decltype(makeGuessTable<Uint, BASE>()) result{};
    for (size_t i = 0; i < result.size(); ++i) {
        Uint pow2 = static_cast<Uint>(Uint{1} << i);
        result.data()[i] = logFloor_naive(pow2, BASE);
    }
    return result;
}

// The maximum possible exponent for a given base that can still be represented
// by a given integer type.
// Example: maxExp<uint8_t, 10> = 2, because 10^2 is representable by an 8-bit unsigned
// integer but 10^3 isn't.
template <typename Uint, unsigned BASE>
constexpr Uint maxExp = logFloor_naive<Uint>(static_cast<Uint>(~Uint{0u}), BASE);

// the size of the table is maxPow<Uint, BASE> + 2 because we need to store the maximum power
// +1 because we need to contain it, we are dealing with a size, not an index
// +1 again because for narrow integers, we access guess+1
template <typename Uint, size_t BASE>
constexpr std::array<uint64_t, maxExp<Uint, BASE> + 2> makePowerTable() {
    decltype(makePowerTable<Uint, BASE>()) result{};
    uint64_t x = 1;
    for (size_t i = 0; i < result.size(); ++i, x *= BASE) {
        result.data()[i] = x;
    }
    return result;
}

// If our base is a power of 2, we can convert between the
// logarithms of different bases without losing any precision.
constexpr bool isPow2or0(uint64_t val) {
    return (val & (val - 1)) == 0;
}


template <size_t BASE = 10, typename Uint>
constexpr Uint logFloor(Uint val) {
    if constexpr (isPow2or0(BASE)) {
        return log2floor(val) / log2floor(BASE);
    } else {
        constexpr auto guesses = makeGuessTable<Uint, BASE>();
        constexpr auto powers = makePowerTable<Uint, BASE>();

        uint8_t guess = guesses[log2floor(val)];

        // Accessing guess + 1 isn't always safe for 64-bit integers.
        // This is why we need this condition. See below for more details.
        if constexpr (sizeof(Uint) < sizeof(uint64_t) || guesses.back() + 2 < powers.size()) {
            return guess + (val >= powers[guess + 1]);
        } else {
            return guess + (val / BASE >= powers[guess]);
        }
    }
}

template <size_t BASE = 1024,
          std::enable_if_t<BASE == 1000 || BASE == 1024, int> = 0>
std::string stringifyFileSize(uint64_t size, unsigned precision = 0) noexcept {
    constexpr const char FILE_SIZE_UNITS[8][3]{
        "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"};

    // The linked post about computing the integer logarithm
    // explains how to compute this.
    // This is equivalent to making a table: {1, 1000, 1000 * 1000, ...}
    // or {1, 1024, 1024 * 1024, ...}
    constexpr auto powers = makePowerTable<uint64_t, BASE>();

    unsigned unit = logFloor<BASE>(size);

    // Your numerator is size, your denominator is 1000^unit or 1024^unit.
    std::string result = stringifyFraction(size, powers[unit], precision);
    result.reserve(result.size() + 5);

    // Optional: Space separating number from unit. (usually looks better)
    result.push_back(' ');
    char first = FILE_SIZE_UNITS[unit][0];
    // Optional: Use lower case (kB, mB, etc.) for decimal units
    if constexpr (BASE == 1000) {
        first += 'a' - 'A';
    }
    result.push_back(first);

    // Don't insert anything more in case of single bytes.
    if (unit != 0) {
        if constexpr (BASE == 1024) {
            result.push_back('i');
        }
        result.push_back(FILE_SIZE_UNITS[unit][1]);
    }

    return result;
}

class ThreadPool {
    std::vector<std::thread> threads;

public:
    ~ThreadPool() {
        wait();
    }

    template <typename Fn>
    ThreadPool& operator()(Fn&& fn) {
        threads.emplace_back(fn);
        return *this;
    }

    template <typename Fn>
    void run(Fn&& fn) {
        threads.emplace_back(fn);
    }

    void wait() {
        for (auto& t : threads) {
            t.join();
        }
        threads.clear();
    }
};