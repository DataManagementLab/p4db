#pragma once

#include "counter.hpp"
#include "cycles.hpp"
#include "periodic.hpp"


struct WorkerContext {
    inline static thread_local WorkerContext* context;

    static void init();

    static void deinit();

    struct guard {
        guard();
        ~guard();
    };

    static WorkerContext& get(); // requires constructed object in thread_local


    uint32_t tid;
    stats::Counter cntr;
    stats::Cycles cycl;
    stats::Periodic pcntr;
};
