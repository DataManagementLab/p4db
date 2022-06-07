#include "tuple_put_res.hpp"

#include "db/config.hpp"


TuplePutResHandler::TuplePutResHandler() : counts(Config::instance().num_txn_workers) {}


void TuplePutResHandler::add(size_t index) {
    counts[index].incr();
}

void TuplePutResHandler::handle(msg::node_t node) {
    counts[node.get_tid()].decr();
}

void TuplePutResHandler::wait(size_t index) {
    counts[index].wait_zero();
}

void TuplePutResHandler::Counter::incr() {
    cnt.fetch_add(1);
}
void TuplePutResHandler::Counter::decr() {
    cnt.fetch_sub(1);
}
void TuplePutResHandler::Counter::wait_zero() {
    while (cnt.load(std::memory_order_relaxed) != 0) {
        __builtin_ia32_pause();
    }
}
