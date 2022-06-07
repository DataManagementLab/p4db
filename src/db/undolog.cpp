#include "undolog.hpp"

#include "comm/msg_handler.hpp"


/* Private methods */

void Undolog::clear(const timestamp_t ts) {
    for (auto& action : actions) {
        action->clear(comm, tid, ts);
    }
    pool.clear();
    actions.clear();
    comm->handler->putresponses.wait(tid); // wait for all remote responses
}


void Undolog::clear_last_n(const timestamp_t ts, const size_t n) {
    if (actions.size() < n) {
        throw std::runtime_error("tried clearing more in undolog than that is there...");
    }
    for (size_t i = 0; i < n; i++) {
        auto action = actions.back();
        action->clear(comm, tid, ts);
        actions.pop_back();
    }
    // putresponses += 1 on remote.clear()
    comm->handler->putresponses.wait(tid); // wait for all remote responses
}