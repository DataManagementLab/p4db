#include "context.hpp"


void WorkerContext::init() {
    context = new WorkerContext();
}

void WorkerContext::deinit() {
    if (context) {
        delete context;
    }
}

WorkerContext::guard::guard() {
    WorkerContext::init();
}
WorkerContext::guard::~guard() {
    WorkerContext::deinit();
}

WorkerContext& WorkerContext::get() { // requires constructed object in thread_local
    return *context;
}
