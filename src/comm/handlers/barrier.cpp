#include "barrier.hpp"

#include "db/config.hpp"


BarrierHandler::BarrierHandler(Communicator* comm) : comm(comm) {
    auto& config = Config::instance();
    pthread_barrier_init(&local_barrier, nullptr, config.num_txn_workers);
    num_nodes = comm->num_nodes;
}

BarrierHandler::~BarrierHandler() {
    pthread_barrier_destroy(&local_barrier);
}

void BarrierHandler::handle() {
    received.fetch_add(1, std::memory_order_relaxed);
}

void BarrierHandler::wait_nodes() {
    wait();
}

void BarrierHandler::wait_workers() {
    uint32_t my_wait = local.fetch_add(1);
    pthread_barrier_wait(&local_barrier);
    if (my_wait == 0) { // execute only by one
        wait();
    }
    pthread_barrier_wait(&local_barrier);
}

// private
void BarrierHandler::wait() {
    for (uint32_t i = 0; i < num_nodes; ++i) {
        auto pkt = comm->make_pkt();
        auto msg = pkt->ctor<msg::Barrier>();
        msg->sender = comm->node_id;
        comm->send(msg::node_t{i}, pkt);
    }

    while (received.load(std::memory_order_relaxed) < num_nodes) {
        __builtin_ia32_pause();
    }

    std::cerr << "Barrier wait done.\n";
    // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    local.store(0, std::memory_order_release);
    received.fetch_sub(num_nodes, std::memory_order_release);
}