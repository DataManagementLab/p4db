#pragma once


#include "comm/msg.hpp"
#include "db/defs.hpp"
#include "db/errors.hpp"
#include "server.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>


struct MessageHandler;


struct UDPPacketBuffer {
    UDPPacketBuffer() = delete;
    ~UDPPacketBuffer() = delete;

    UDPPacketBuffer(const UDPPacketBuffer& other) = delete;
    UDPPacketBuffer(UDPPacketBuffer&& other) = delete;

    UDPPacketBuffer& operator=(const UDPPacketBuffer& other) = delete;
    UDPPacketBuffer& operator=(UDPPacketBuffer&& other) = delete;


    static constexpr std::size_t BUF_SIZE = 1500;

    uint8_t buffer[BUF_SIZE]; // size stored at end
    int len = 0;

    static auto alloc() {
        void* data = std::malloc(BUF_SIZE + sizeof(int)); // MTU for now
        return static_cast<UDPPacketBuffer*>(data);
    }

    template <typename T, typename... Args>
    auto ctor(Args&&... args) {
        len = sizeof(T);
        return new (this) T{std::forward<Args>(args)...};
    }

    template <typename T>
    auto as() {
        return reinterpret_cast<T*>(this);
    }

    void resize(const std::size_t len) {
        if (len > BUF_SIZE) {
            throw error::PacketBufferTooSmall();
        }
        this->len = len;
    }

    auto size() {
        return len;
    }

    operator uint8_t*() {
        return reinterpret_cast<uint8_t*>(this);
    }

    void free() {
        std::free(this);
    }

    void dump(std::ostream& os) {
        auto bytes = as<uint8_t>();
        hex_dump(os, bytes, size());
    }
};


class UDPCommunicator {
    // using lock_t = std::mutex;
    using lock_t = SpinLock;

    lock_t mutex;

    int sock;
    UDPPacketBuffer* recv_buffer = nullptr;

public:
    using Pkt_t = UDPPacketBuffer;

    std::vector<struct sockaddr_in> addresses;
    msg::node_t node_id;
    msg::node_t switch_id;
    uint32_t num_nodes;
    MessageHandler* handler = nullptr;
    std::jthread thread;


    // uint16_t num_rx_queues;
    // uint16_t num_tx_queues;
    uint32_t mh_tid;
    // uint16_t spin_tx_queue;


public:
    UDPCommunicator();

    ~UDPCommunicator();


    void set_handler(MessageHandler* handler);

    void send(msg::node_t target, UDPPacketBuffer*& pkt);
    void send(msg::node_t target, Pkt_t*& pkt, uint32_t);


    UDPPacketBuffer* make_pkt();

private:
    UDPPacketBuffer* receive();
    void setup(uint16_t port);
};