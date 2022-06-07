#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <chrono>
#include <iostream>
#include <mutex>
#include <sys/mman.h>
#include <sched.h>


constexpr size_t NUM_TXN = 10'000'000;
constexpr size_t TABLE_SIZE = 43008;
#ifndef NUM_PARTITIONS
constexpr size_t NUM_PARTITIONS = 48;
#endif

// #define ROW_LOCK




struct Table {
#ifdef ROW_LOCK
    std::mutex mutex; 
#endif
    uint32_t value;
};

enum class OP {
    READ = 0,
    WRITE = 1,
};

struct Transaction {
    struct {
        OP op;
        uint32_t key;
        uint32_t value;
    } part[NUM_PARTITIONS];
};


template<typename Function>
auto timeit(Function function, const char *name, bool print=true) {
    auto start = std::chrono::high_resolution_clock::now();

    function();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (print) std::cout << name << "    took: " << duration << "µs\n";
    return duration;
}

void pin_to_core(int core, pthread_t pid=pthread_self()) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(core, &mask);
        
        if (pthread_setaffinity_np(pid, sizeof(cpu_set_t), &mask) != 0) {
            perror("pthread_setaffinity_np");
        }
    }

template <typename T>
void do_not_optimize(T const& val) {
  asm volatile("" : : "g"(val) : "memory");
}

// g++ -std=c++17 -g -O3 -march=native -pthread nolock_test.cpp -o nolock_test && ./nolock_test
int main(int, char**) {
    pin_to_core(0);

    std::array<Table*, NUM_PARTITIONS> tables;
    for (size_t j = 0; j < NUM_PARTITIONS; j++) {
		tables[j] = (Table*) mmap(nullptr, sizeof(Table) * TABLE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        for (size_t i = 0; i < TABLE_SIZE; i++) {
            tables[j][i].value = std::rand();
        }
    }

    Transaction *txns = (Transaction*) mmap(nullptr, sizeof(Transaction) * NUM_TXN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    for (size_t i = 0; i < NUM_TXN; i++) {
        for (size_t j = 0; j < NUM_PARTITIONS; j++) {
            txns[i].part[j].op = static_cast<OP>(std::rand() % 2);
            txns[i].part[j].key = std::rand() % TABLE_SIZE;
            txns[i].part[j].value = std::rand();
        }
    }

    double duration = timeit([&]() {
        uint64_t read_sum = 0;
        for (size_t i = 0; i < NUM_TXN; i++) {
            #ifdef ROW_LOCK
            std::array<std::unique_lock<std::mutex>, NUM_PARTITIONS> locks;
            for (size_t j = 0; j < NUM_PARTITIONS; j++) {
                auto &table = tables[j];
                auto &part = txns[i].part[j];
                locks[j] = std::unique_lock<std::mutex>(table[part.key].mutex);
            }
            for (size_t j = 0; j < NUM_PARTITIONS; j++) {
                auto &table = tables[j];
                auto &part = txns[i].part[j];
                if (part.op == OP::WRITE) {
                    table[part.key].value = part.value;
                } else {
                    read_sum += table[part.key].value;
                }
            }
            for (size_t j = 0; j < NUM_PARTITIONS; j++) {
                locks[j].unlock();
            }
            #else
            for (size_t j = 0; j < NUM_PARTITIONS; j++) {
                auto &table = tables[j];
                auto &part = txns[i].part[j];

                if (part.op == OP::WRITE) {
                    table[part.key].value = part.value;
                } else {
                    read_sum += table[part.key].value;
                }
            }
            #endif
        }
        do_not_optimize(read_sum);
    }, "run txns");

    std::cout << "Txn/sec: " << static_cast<uint64_t>(NUM_TXN / (duration / 1'000'000)) << '\n';

    return 0;
}