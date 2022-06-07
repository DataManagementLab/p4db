#pragma once

#include <algorithm>
#include <cstdint>


template <typename T, typename Total, std::size_t N>
struct MovingAverage {
    static_assert(N && ((N & (N - 1)) == 0), "N not power of 2");

    void add(T sample) {
        if (size < N) {
            samples[size++] = sample;
            total_sum += sample;
        } else {
            T& oldest = samples[size++ % N];
            total_sum += sample - oldest;
            oldest = sample;
        }
    }

    double avg() const {
        if (size == 0) {
            return 0.0;
        }
        return total_sum / std::min(size, N);
    }

    size_t size{0};
    Total total_sum{0};
    T samples[N];
};