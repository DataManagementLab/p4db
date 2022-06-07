#include "comm/bigendian.hpp"
#include "comm/eth_hdr.hpp"
#include "comm/msg.hpp"
#include "db/buffers.hpp"
#include "db/hex_dump.hpp"
#include "network_interface.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>


template <typename T>
struct pkt_t {
    eth_hdr_t eth;
    T msg;

    template <typename... Args>
    pkt_t(Args&&... args)
        : msg{std::forward<Args>(args)...} {}
} /*__attribute__((packed))*/;


NetworkInterface net{"enp1s0f1"};


int mode_0() {
    // timestamp_t ts, p4db::table_t tid, p4db::key_t rid, AccessMode mode
    AccessMode mode = AccessMode::WRITE;
    mode.set_switch_index(0x1234);


    pkt_t<msg::TupleGetReq> pkt{timestamp_t{0xaaaaaaaaaaaaaaaa}, p4db::table_t{0xbbbbbbbbbbbbbbbb}, p4db::key_t{0xcccccccccccccccc}, mode};
    pkt.eth.type = ETHER_TYPE;
    net.set_src_mac(pkt);
    // pkt.eth.dst = pkt.eth.src;
    pkt.eth.dst = {0xac, 0x1f, 0x6b, 0x41, 0x65, 0x35}; // node2

    uint16_t len = static_cast<uint16_t>(sizeof(pkt));
    std::cout << "sent packet: " << len << '\n';
    hex_dump(std::cerr, reinterpret_cast<uint8_t*>(&pkt), len);
    net.send_pkt(pkt, len);


    net.recv_pkt<pkt_t<msg::TupleGetRes>>([&](const auto& pkt, int len) {
        std::cout << "received: " << len << '\n';
        hex_dump(std::cerr, reinterpret_cast<const uint8_t*>(&pkt), len);

        std::cout << "is_TupleGetRes=" << (pkt.msg.type == msg::TupleGetRes::MSG_TYPE) << '\n';
        std::cout << "mode=" << pkt.msg.mode.value << '\n';
        std::cout << "mode=" << pkt.msg.mode.get_clean() << '\n';
        std::cout << "by_switch=" << pkt.msg.mode.by_switch() << '\n';
    });

    return 0;
}


int mode_1() {
    // timestamp_t ts, p4db::table_t tid, p4db::key_t rid, AccessMode mode
    AccessMode mode = AccessMode::WRITE;
    mode.set_switch_index(0x1234);

    uint8_t buffer[1500];
    auto pkt = new (buffer) pkt_t<msg::TuplePutReq>{timestamp_t{0xaaaaaaaaaaaaaaaa}, p4db::table_t{0xbbbbbbbbbbbbbbbb}, p4db::key_t{0xcccccccccccccccc}, mode};
    pkt->eth.type = ETHER_TYPE;
    net.set_src_mac(*pkt);
    // pkt->eth.dst = pkt->eth.src;
    pkt->eth.dst = {0xac, 0x1f, 0x6b, 0x41, 0x65, 0x35}; // node2

    uint32_t tuples[4] = {0x1122344, 0xaabbccdd, 0x21436587, 0xafafafaf};
    std::memcpy(pkt->msg.tuple, tuples, sizeof(tuples));

    uint16_t len = sizeof(eth_hdr_t) + 2 + msg::TuplePutReq::size(sizeof(tuples));
    std::cout << "sent packet: " << len << '\n';
    hex_dump(std::cerr, reinterpret_cast<uint8_t*>(pkt), len);
    net.send_pkt(*pkt, len);


    net.recv_pkt<pkt_t<msg::TuplePutRes>>([&](const auto& pkt, int len) {
        std::cout << "received: " << len << '\n';
        hex_dump(std::cerr, reinterpret_cast<const uint8_t*>(&pkt), len);

        std::cout << "is_TuplePutRes=" << (pkt.msg.type == msg::TuplePutRes::MSG_TYPE) << '\n';
        std::cout << "mode=" << pkt.msg.mode.value << '\n';
        std::cout << "mode=" << pkt.msg.mode.get_clean() << '\n';
        std::cout << "by_switch=" << pkt.msg.mode.by_switch() << '\n';
    });


    return 0;
}


int mode_2() {
    // timestamp_t ts, p4db::table_t tid, p4db::key_t rid, AccessMode mode
    pkt_t<msg::TuplePutReq> pkt{timestamp_t{0xaaaaaaaaaaaaaaaa}, p4db::table_t{0xbbbbbbbbbbbbbbbb}, p4db::key_t{0xcccccccccccccccc}, AccessMode::WRITE};
    pkt.eth.type = ETHER_TYPE;
    net.set_src_mac(pkt);
    // pkt.eth.dst = pkt.eth.src;
    pkt.eth.dst = {0xac, 0x1f, 0x6b, 0x41, 0x65, 0x35}; // node2

    uint16_t len = static_cast<uint16_t>(sizeof(pkt));
    std::cout << "sent packet: " << len << '\n';
    hex_dump(std::cerr, reinterpret_cast<uint8_t*>(&pkt), len);
    net.send_pkt(pkt, len);


    net.recv_pkt<pkt_t<msg::TuplePutRes>>([&](const auto& pkt, int len) {
        std::cout << "received: " << len << '\n';
        hex_dump(std::cerr, reinterpret_cast<const uint8_t*>(&pkt), len);

        std::cout << "is_TuplePutRes=" << (pkt.msg.type == msg::TuplePutRes::MSG_TYPE) << '\n';
        std::cout << "mode=" << static_cast<int>(pkt.msg.mode) << '\n';
    });

    return 0;
}


int mode_3() {
    net.recv_pkt<pkt_t<msg::TuplePutReq>>([&](auto& pkt, int len) {
        std::cout << "received: " << len << '\n';
        hex_dump(std::cerr, reinterpret_cast<const uint8_t*>(&pkt), len);

        std::cout << "is_TuplePutReq=" << (pkt.msg.type == msg::TuplePutReq::MSG_TYPE) << '\n';
        std::cout << "mode=" << static_cast<int>(pkt.msg.mode) << '\n';

        pkt.msg.template convert<msg::TuplePutRes>();
        pkt.eth.dst = pkt.eth.src;
        net.set_src_mac(pkt);
        net.send_pkt(pkt, len);
    });

    return 0;
}


int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mode=0/1>" << '\n';
        return -1;
    }


    switch (std::stoi(argv[1])) {
        case 0:
            return mode_0();
        case 1:
            return mode_1();
        case 2:
            return mode_2();
        case 3:
            return mode_3();
        default:
            std::cerr << "Unkown mode" << '\n';
            return -1;
    }

    return 0;
}
