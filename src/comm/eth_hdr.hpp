#pragma once

#include <ostream>
// #include <fmt/core.h>
// #include <fmt/compile.h>


#include "bigendian.hpp"


struct eth_addr_t {
    uint8_t addr_bytes[6];

    bool operator==(const eth_addr_t& other) const {
        const uint16_t* w1 = (const uint16_t*)addr_bytes;
        const uint16_t* w2 = (const uint16_t*)other.addr_bytes;

        return ((w1[0] ^ w2[0]) | (w1[1] ^ w2[1]) | (w1[2] ^ w2[2])) == 0;
    }

    friend std::ostream& operator<<(std::ostream& os, const eth_addr_t& a) {
        // os << fmt::format(FMT_COMPILE("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}"),
        //     a.addr_bytes[0], a.addr_bytes[1], a.addr_bytes[2],
        //     a.addr_bytes[3], a.addr_bytes[4], a.addr_bytes[5]
        // );
        (void)a;
        return os;
    }
} __attribute__((packed));
static_assert(sizeof(eth_addr_t) == 6);


struct eth_hdr_t {
    eth_addr_t dst;
    eth_addr_t src;
    be_uint16_t type;

    friend std::ostream& operator<<(std::ostream& os, const eth_hdr_t& p) {
        // os << "Src: " << p.src;
        // os << " --> Dst: " << p.dst;
        // os << fmt::format(FMT_COMPILE(" (Type: 0x{:04x})"), *p.type);
        (void)p;
        return os;
    }
} __attribute__((packed));
static_assert(sizeof(eth_hdr_t) == 14);