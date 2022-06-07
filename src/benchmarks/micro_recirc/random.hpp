#pragma once

#include <cstdint>
#include <cstring>
#include <random>


namespace benchmark {
namespace micro_recirc {


class MicroRecircRandom {
public:
    using RandomDevice = std::mt19937;

private:
    RandomDevice gen;
    Config& config;

public:
    MicroRecircRandom(uint32_t seed)
        : gen(seed), config(Config::instance()) {}

    template <typename T>
    T random(T lower, T upper) {
        std::uniform_int_distribution<T> dist(lower, upper);
        return dist(gen);
    }

    bool is_multipass() {
        return random<int>(1, 100) <= config.micro_recirc.recirc_prob;
    }
};


} // namespace micro_recirc
} // namespace benchmark
