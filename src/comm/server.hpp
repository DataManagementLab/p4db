#pragma once


#include "eth_hdr.hpp"

#include <cstdint>
#include <string>


struct Server {
    std::string ip;
    uint16_t port;
    eth_addr_t mac;
};
