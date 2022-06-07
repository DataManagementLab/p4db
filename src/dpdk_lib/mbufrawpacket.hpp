#pragma once


#include "rte_errno.h"
#include "rte_mbuf.h"
#include "rte_mempool.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <new>
#include <stddef.h>
#include <stdexcept>
#include <sys/time.h>
#include <vector>


class DpdkDevice;


struct DPDKPacket : private rte_mbuf {
    // struct EmptyDeleter {
    //     void operator()(DPDKPacket*) const {}
    // };
    // using dpdk_pkt_ptr = std::unique_ptr<DPDKPacket, EmptyDeleter>;

    DPDKPacket() = delete;
    ~DPDKPacket() = default;

    DPDKPacket(const DPDKPacket& other) = delete;
    DPDKPacket(DPDKPacket&& other) = delete;

    DPDKPacket& operator=(const DPDKPacket& other) = delete;
    DPDKPacket& operator=(DPDKPacket&& other) = delete;

    static DPDKPacket* wrap(struct rte_mbuf* mbuf);

    struct rte_mbuf* raw();

    uint8_t* reserve(uint16_t len);

    // void trim(uint16_t len);

    uint8_t* data();

    uint16_t size();

    void free();
};
static_assert(sizeof(DPDKPacket) == sizeof(struct rte_mbuf));
