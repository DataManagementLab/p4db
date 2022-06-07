#include "benchmarks/micro_recirc/switch.hpp"
#include "comm/bigendian.hpp"
#include "comm/msg.hpp"
#include "db/buffers.hpp"
#include "db/hex_dump.hpp"
#include "network_interface.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>


struct pkt_t {
    eth_hdr_t eth;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    msg::Header msg[0];
#pragma GCC diagnostic pop
} /*__attribute__((packed))*/;


NetworkInterface net{"enp1s0f1"};

template <typename BW_CB, typename BR_CB>
void send_recv(BW_CB&& bw_cb, BR_CB&& br_cb) {
    auto pkt = reinterpret_cast<pkt_t*>(new uint8_t[1500]{});
    pkt->eth.type = ETHER_TYPE;
    net.set_src_mac(*pkt);
    pkt->eth.dst = {0x1B, 0xAD, 0xC0, 0xDE, 0xBA, 0xBE};
    // std::memcpy(&pkt->eth.dst, &pkt->eth.src, ETH_ALEN);

    auto txn = new (pkt->msg) msg::SwitchTxn();
    BufferWriter bw{txn->data};

    bw_cb(bw); // fill pkt

    uint16_t len = static_cast<uint16_t>(sizeof(pkt_t) + msg::SwitchTxn::size(bw.size));
    std::cout << "sent packet: " << len << '\n';
    hex_dump(std::cerr, reinterpret_cast<uint8_t*>(pkt), len);
    net.send_pkt(*pkt, len);
    delete[] pkt;

    net.recv_pkt<pkt_t>([&](const auto& pkt, int len) {
        std::cout << "received: " << len << '\n';
        hex_dump(std::cerr, reinterpret_cast<const uint8_t*>(&pkt), len);

        auto txn = reinterpret_cast<const msg::SwitchTxn*>(pkt.msg);
        BufferReader br{const_cast<uint8_t*>(txn->data)};

        br_cb(br);
    });
}


int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (argc < 2) {
        throw std::invalid_argument("./main <#recircs>");
    }


    using namespace benchmark::micro_recirc;


    const uint32_t recircs = std::stoi(argv[1]);

    send_recv([&](auto& bw) {
        auto info = bw.write(info_t{});
        bw.write(recirc_t{});

        for (uint32_t i = 0; i < recircs; ++i) {
            auto instr = bw.write(recirc_t{});
            instr->type.set_stop(true);
        }
        if (recircs > 0) {
            info->multipass = 1;
            info->locks = lock_t{1, 1};
        }

        bw.write(InstrType_t::STOP()); }, [&](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        for (uint32_t i = 0; i == 0 || i < recircs; ++i) {
            auto recirc = br.template read<recirc_t>();
            std::cout << *recirc << '\n';
        } });


    return 0;
}