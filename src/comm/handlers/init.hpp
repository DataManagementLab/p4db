#pragma once

#include "comm/comm.hpp"


struct InitHandler {
    Communicator* comm;
    uint32_t num_nodes;
    std::vector<bool> nodes;

    InitHandler(Communicator* comm);

    void handle(msg::node_t node);
    void wait();
};
