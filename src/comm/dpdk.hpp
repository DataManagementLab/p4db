#pragma once


#include "comm/msg.hpp"
#include "db/errors.hpp"
#include "db/hex_dump.hpp"
#include "db/spinlock.hpp"
#include "dpdk_lib/dpdk.hpp"
#include "server.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>


struct MessageHandler;


struct DPDKPacketBuffer : public DPDKPacket {
    DPDKPacketBuffer() = delete;
    ~DPDKPacketBuffer() = delete;

    DPDKPacketBuffer(const DPDKPacketBuffer& other) = delete;
    DPDKPacketBuffer(DPDKPacketBuffer&& other) = delete;

    DPDKPacketBuffer& operator=(const DPDKPacketBuffer& other) = delete;
    DPDKPacketBuffer& operator=(DPDKPacketBuffer&& other) = delete;


    static auto alloc(DpdkDevice* device) {
        auto dpdk_pkt = device->allocate();
        return static_cast<DPDKPacketBuffer*>(dpdk_pkt);
    }

    struct pkt_t {
        eth_hdr_t eth;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        msg::Header msg[0];
#pragma GCC diagnostic pop
    } /*__attribute__((packed))*/;


    template <typename T, typename... Args>
    auto ctor(Args&&... args) {
        auto dpdk_pkt = static_cast<DPDKPacket*>(this);
        uint8_t* data = dpdk_pkt->reserve(sizeof(pkt_t::eth) + sizeof(T)); // TODO account for alignment
        pkt_t* pkt = reinterpret_cast<pkt_t*>(data);

        return new (pkt->msg) T{std::forward<Args>(args)...};
    }

    // #pragma GCC diagnostic push
    // #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    template <typename T>
    auto as() {
        pkt_t* pkt = reinterpret_cast<pkt_t*>(data());
        return reinterpret_cast<T*>(pkt->msg);
    }
    // #pragma GCC diagnostic pop

    void resize(const std::size_t len) {
        // throw error::PacketBufferTooSmall();
        auto dpdk_pkt = static_cast<DPDKPacket*>(this);
        dpdk_pkt->reserve(sizeof(pkt_t) + len);
    }

    void dump(std::ostream& os) {
        auto bytes = data();
        hex_dump(os, bytes, size());
    }

private:
    template <typename... Args>
    void ctor_eth(Args&&... args) {
        pkt_t* pkt = reinterpret_cast<pkt_t*>(data());
        new (&pkt->eth) eth_hdr_t{std::forward<Args>(args)...};
    }

    friend class DPDKCommunicator;
};


class DPDKCommunicator {
    // using lock_t = std::mutex;
    using lock_t = SpinLock;

    lock_t mutex;

public:
    using Pkt_t = DPDKPacketBuffer;

    std::shared_ptr<DpdkDevice> device;

    eth_addr_t src_mac;
    struct target_info {
        eth_addr_t mac;
    };
    std::vector<target_info> targets;

    msg::node_t node_id;
    msg::node_t switch_id;
    uint32_t num_nodes;
    uint16_t num_rx_queues;
    uint16_t num_tx_queues;
    uint32_t mh_tid;
    uint16_t spin_tx_queue;
    MessageHandler* handler = nullptr;
    std::jthread stats_thread;

public:
    DPDKCommunicator();

    ~DPDKCommunicator();

    void set_handler(MessageHandler* handler);

    void send(msg::node_t target, DPDKPacketBuffer*& pkt);
    void send(msg::node_t target, DPDKPacketBuffer*& pkt, uint32_t tid);

    DPDKPacketBuffer* make_pkt();
};


constexpr size_t MAX_RECEIVE_BURST = 64;


class ReceiverThread final : public DpdkWorkerThread {
    std::shared_ptr<DpdkDevice> device;
    const uint16_t rx_queues;

    bool do_stop = false;
    uint32_t core_id;
    uint32_t mh_tid;
    MessageHandler* handler;

public:
    ReceiverThread(std::shared_ptr<DpdkDevice> device, uint16_t rx_queues, uint32_t mh_tid, MessageHandler* handler)
        : device(device), rx_queues(rx_queues), mh_tid(mh_tid), handler(handler) {}

    ~ReceiverThread() = default;

    bool run(uint32_t core_id);
    void stop() { do_stop = true; }
    uint32_t getCoreId() const { return core_id; }
};
