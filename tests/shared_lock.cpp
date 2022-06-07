#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <pthread.h>


struct SharedLock {
    static constexpr uint32_t EX = 0xffffffff;
    static constexpr uint32_t EX_PENDING_MASK = 0x80000000;

    std::atomic<uint32_t> state;

    void lock() {
        uint32_t val = 0;    // try fast-path with 0, then mark as write-pending
        while (!state.compare_exchange_weak(val, EX, std::memory_order_acquire)) {
            state.fetch_or(EX_PENDING_MASK, std::memory_order_relaxed);
            __asm volatile ("pause" ::: );
            val = EX_PENDING_MASK;
        }
    }

    void unlock() {
        state.store(0, std::memory_order_release);
    }

    void lock_shared() {
        uint32_t val;
        do {
            // wait for no pending writes and retrive current reference count
            while ((val = state.load(std::memory_order_relaxed)) & EX_PENDING_MASK) {
                __asm volatile ("pause" ::: );
            }
        } while (!state.compare_exchange_weak(val, val+1, std::memory_order_acquire));
    }


    void unlock_shared() {
        state.fetch_sub(1, std::memory_order_release);
    }
};




int main(int argc, char **argv) {
    (void) argc; (void) argv;

    // g++ -std=c++20 -g -O3 -march=native -pthread shared_lock.cpp -o shared_lock && ./shared_lock

    constexpr size_t num_readers = 2;
    constexpr size_t num_writers = 1;

    SharedLock lock;
    // std::shared_mutex lock;
    bool stop = false;
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_readers+num_writers+1);

    std::atomic<uint64_t> reads{0};
    std::thread readers[num_readers];
    for (size_t i = 0; i < num_readers; i++) {
        readers[i] = std::thread([&]() {
            pthread_barrier_wait(&barrier);
            while (!stop) {
                lock.lock_shared();
                // if (reads % 100000 == 0)
                //     std::cout << "reader lock " + std::to_string(lock.state.load() & ~lock.EX_PENDING_MASK) + '\n';
                ++reads;
                lock.unlock_shared();
            }
        });
    }

    std::atomic<uint64_t> writes{0};
    std::thread writers[num_writers];
    for (size_t i = 0; i < num_writers; i++) {
        writers[i] = std::thread([&]() {
            pthread_barrier_wait(&barrier);
            while(!stop) {
                lock.lock();
                // std::cout << "writer lock\n";
                ++writes;
                lock.unlock();
            }
        });
    }

    pthread_barrier_wait(&barrier);
    std::cout << "Press [ENTER] to stop.\n";
    std::cin.get();
    stop = true;

    for (size_t i = 0; i < num_readers; i++) {
        readers[i].join();
    }
    for (size_t i = 0; i < num_writers; i++) {
        writers[i].join();
    }

    std::cout << "writes=" << writes << " reads=" << reads << " of that fraction " << ((double) writes) / (writes+reads)  << " writes" << '\n';

    return 0;
}
