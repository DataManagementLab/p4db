#include "utils/zipf.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <random>

// constexpr double g_zipf_theta = 0.6;
// constexpr uint64_t the_n = 43'000;
// constexpr uint64_t num_values = 1'000'000;

// double zeta(uint64_t n, double theta) {
// 	double sum = 0;
// 	for (uint64_t i = 1; i <= n; i++)
// 		sum += pow(1.0 / i, theta);
// 	return sum;
// }

// double denom = zeta(the_n, g_zipf_theta);
// double zeta_2_theta;
// drand48_data buffer;

// uint64_t zipf(uint64_t n, double theta) {
// 	double alpha = 1 / (1 - theta);
// 	double zetan = denom;
// 	double eta = (1 - pow(2.0 / n, 1 - theta)) /
// 		(1 - zeta_2_theta / zetan);
// 	double u;
// 	drand48_r(&buffer, &u);
// 	double uz = u * zetan;
// 	if (uz < 1) return 1;
// 	if (uz < 1 + pow(0.5, theta)) return 2;
// 	return 1 + (uint64_t)(n * pow(eta * u - eta + 1, alpha));
// }

double rand_val(int seed); // Jain's RNG

//===========================================================================
//=  Function to generate Zipf (power law) distributed random variables     =
//=    - Input: alpha and N                                                 =
//=    - Output: Returns with Zipf distributed random variable              =
//===========================================================================
int zipf(double alpha, int n, bool first = false) {
    //   static int first = TRUE;      // Static first time flag
    static double c = 0;      // Normalization constant
    static double* sum_probs; // Pre-calculated sum of probabilities
    double z;                 // Uniform random number (0 < z < 1)
    int zipf_value;           // Computed exponential value to be returned
    int i;                    // Loop counter
    int low, high, mid;       // Binary-search bounds

    // Compute normalization constant on first call only
    if (first) {
        for (i = 1; i <= n; i++)
            c = c + (1.0 / pow((double)i, alpha));
        c = 1.0 / c;

        sum_probs = (double*)malloc((n + 1) * sizeof(*sum_probs));
        sum_probs[0] = 0;
        for (i = 1; i <= n; i++) {
            sum_probs[i] = sum_probs[i - 1] + c / pow((double)i, alpha);
        }
    }

    // Pull a uniform random number (0 < z < 1)
    do {
        z = rand_val(0);
    } while ((z == 0) || (z == 1));

    // Map z to the value
    low = 1, high = n, mid;
    do {
        mid = floor((low + high) / 2);
        if (sum_probs[mid] >= z && sum_probs[mid - 1] < z) {
            zipf_value = mid;
            break;
        } else if (sum_probs[mid] >= z) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    } while (low <= high);

    // Assert that zipf_value is between 1 and N
    assert((zipf_value >= 1) && (zipf_value <= n));

    return (zipf_value);
}

//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
double rand_val(int seed) {
    const long a = 16807;      // Multiplier
    const long m = 2147483647; // Modulus
    const long q = 127773;     // m div a
    const long r = 2836;       // m mod a
    static long x;             // Random int value
    long x_div_q;              // x divided by q
    long x_mod_q;              // x modulo q
    long x_new;                // New x value

    // Set the seed if argument is non-zero and then return zero
    if (seed > 0) {
        x = seed;
        return (0.0);
    }

    // RNG using integer arithmetic
    x_div_q = x / q;
    x_mod_q = x % q;
    x_new = (a * x_mod_q) - (r * x_div_q);
    if (x_new > 0)
        x = x_new;
    else
        x = x_new + m;

    // Return a random value between 0.0 and 1.0
    return ((double)x / m);
}

// g++ -std=c++17 -g -O3 -march=native -pthread zipf.cpp -o zipf && ./zipf

int main(void) {
    // std::cout << "value,theta\n";
    // for (uint64_t i = 0; i < num_values; i++) {
    //     std::cout << zipf(the_n, g_zipf_theta) << ',' << g_zipf_theta << '\n';
    // }

    std::cout << "theta, value\n";
    for (double theta = 0.0; theta <= 2.0; theta += 0.1) {
        std::cerr << "theta=" << theta << '\n';

        constexpr uint64_t table_size = 1024;
        std::random_device rd;
        std::mt19937 gen(0); //rd());
        zipf_distribution zipf_dist{table_size - 1, theta};

        for (uint64_t i = 0; i < 100'000; i++) {
            std::cout << "θ=" << theta << ',' << zipf_dist(gen) << '\n';
        }

        // rand_val(42);
        // zipf(theta, table_size, true);
        // for (uint64_t i = 0; i < 100'000; i++) {
        //     std::cout << "θ=" << theta << ',' << zipf(theta, table_size) << '\n';
        // }
    }

    return 0;
}