#include "udp.hpp"

#include "db/config.hpp"
#include "msg_handler.hpp"


UDPCommunicator::UDPCommunicator() {
    auto& config = Config::instance();
    node_id = config.node_id;
    num_nodes = config.num_nodes;
    switch_id = config.switch_id;
    mh_tid = config.num_txn_workers;


    setup(config.servers.at(node_id).port);

    addresses.reserve(config.servers.size());
    for (auto& server : config.servers) {
        auto& client_addr = addresses.emplace_back();

        client_addr.sin_family = AF_INET;
        inet_pton(AF_INET, server.ip.c_str(), &client_addr.sin_addr);
        client_addr.sin_port = htons(server.port);
    }
}

UDPCommunicator::~UDPCommunicator() {
    shutdown(sock, SHUT_RDWR);
    close(sock);
    if (recv_buffer) {
        recv_buffer->free();
    }
}


void UDPCommunicator::set_handler(MessageHandler* handler) {
    this->handler = handler;
    auto& config = Config::instance();
    thread = std::jthread([&, handler](std::stop_token token) {
        pin_worker(config.num_txn_workers);
        while (!token.stop_requested()) {
            auto pkt = receive();
            if (!pkt) {
                continue;
            }
            handler->handle(pkt);
        }
    });
}

void UDPCommunicator::send(msg::node_t target, Pkt_t*& pkt, uint32_t) {
    return send(target, pkt);
}

void UDPCommunicator::send(msg::node_t target, Pkt_t*& pkt) {
    if (target >= addresses.size()) {
        throw std::runtime_error("target " + std::to_string(target) + " out of bounds");
    }

    int len;
    {
        const std::lock_guard<lock_t> lock(mutex); // Interestingly locking is faster
        len = sendto(sock, pkt, pkt->size(), 0, (const struct sockaddr*)&addresses[target], sizeof(struct sockaddr_in));
    }

    if (len != pkt->size()) {
        std::perror("sendto failed");
        std::exit(EXIT_FAILURE);
    }

    pkt->free();
    pkt = nullptr; // to detect cause segfault on write
}


UDPCommunicator::Pkt_t* UDPCommunicator::make_pkt() {
    return UDPPacketBuffer::alloc();
}


/* Private Methods */

void UDPCommunicator::setup(uint16_t port) {
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        std::exit(EXIT_FAILURE);
    }

    if (bind(sock, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        std::exit(EXIT_FAILURE);
    }
}


UDPCommunicator::Pkt_t* UDPCommunicator::receive() {
    if (!recv_buffer) {
        recv_buffer = Pkt_t::alloc();
    }
    int len = recv(sock, recv_buffer, Pkt_t::BUF_SIZE, MSG_DONTWAIT);
    if (len <= 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        return nullptr;
    }
    if (len == 0) {
        return nullptr;
    }
    if (len == -1) { // socket close
        return nullptr;
    }
    recv_buffer->len = len;
    auto msg = recv_buffer;
    recv_buffer = nullptr;
    return msg;
}