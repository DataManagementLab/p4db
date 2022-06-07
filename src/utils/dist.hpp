#pragma once

#include <random>
#include <stdexcept>


template <typename T>
struct UniformRemoteRank {
    T max;
    T except;
    std::uniform_int_distribution<T> dist;

    explicit UniformRemoteRank(T max, T except) : max(max), except(except), dist(0, max - 2) {
        if (except > max - 1) {
            throw std::invalid_argument("except > max-1");
        }
    }

    template <typename U>
    T operator()(U& rng) {
        T rank = dist(rng);
        if (rank == except) {
            return max - 1;
        }
        return rank;
    }
};

template <typename T>
struct PercentDist {
    T threshold;
    std::uniform_int_distribution<T> dist{0, 99};

    PercentDist(T threshhold) : threshold(threshhold) {}

    template <typename U>
    bool operator()(U& rng) {
        return dist(rng) < threshold;
    }
};
