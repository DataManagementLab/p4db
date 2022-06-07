#pragma once

#include <cstdint>
#include <cstring>
#include <random>


namespace benchmark {
namespace tpcc {


// Stuff for generating random input
class TPCCRandom {
public:
    using RandomDevice = std::mt19937;

private:
    RandomDevice gen;

public:
    TPCCRandom(uint32_t seed) : gen(seed) {}

    void astring(int x, int y, char* s) {
        static constexpr const char* alphanum = "0123456789"
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                "abcdefghijklmnopqrstuvwxyz";
        int len = random<int>(x, y);
        for (int i = 0; i < len; ++i) {
            s[i] = alphanum[random<int>(0, 61)];
        }
        s[len] = '\0';
    }
    void nstring(int x, int y, char* s) {
        static constexpr const char* numeric = "0123456789";
        int len = random<int>(x, y);
        for (int i = 0; i < len; ++i) {
            s[i] = numeric[random<int>(0, 9)];
        }
        s[len] = '\0';
    }
    void cLastName(int num, char* s) {
        static constexpr const char* n[] = {
            "BAR", "OUGHT", "ABLE", "PRI", "PRES",
            "ESE", "ANTI", "CALLY", "ATION", "EING"};

        strcpy(s, n[num / 100]);
        strcat(s, n[(num / 10) % 10]);
        strcat(s, n[num % 10]);
    }

    template <typename T>
    T random(T lower, T upper) {
        std::uniform_int_distribution<T> dist(lower, upper);
        return dist(gen);
    }

    template <typename T>
    T NURand(T A, T x, T y) {
        constexpr T C = 0;
        return (((random<T>(0, A) | random<T>(x, y)) + C) % (y - x + 1)) + x;
    }
};

} // namespace tpcc
} // namespace benchmark