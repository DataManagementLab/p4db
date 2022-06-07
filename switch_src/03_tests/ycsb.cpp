#include "benchmarks/ycsb/switch.hpp"
#include "comm/bigendian.hpp"
#include "comm/msg.hpp"
#include "db/buffers.hpp"
#include "db/hex_dump.hpp"
#include "network_interface.hpp"

#include <chrono>
#include <experimental/source_location>
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

    using namespace benchmark::ycsb;

    // send_recv([](auto& bw) {
    //     bw.write(info_t{});
    //     bw.write(instr_t{InstrType_t::REG(0), OPCode_t::READ, 0x1234, 0xaaaaaaaa});
    //     bw.write(InstrType_t::STOP());
    // }, [](auto& br) {
    //     auto info = br.template read<info_t>();
    //     std::cout << *info << '\n';
    //     auto instr = br.template read<instr_t>();
    //     std::cout << *instr << '\n';
    // });


    // send_recv([](auto& bw) {
    //     bw.write(info_t{});
    //     bw.write(instr_t{InstrType_t::REG(0), OPCode_t::WRITE, 0x1235, 0xaaaaaaaa});
    //     bw.write(InstrType_t::STOP());
    // }, [](auto& br) {
    //     auto info = br.template read<info_t>();
    //     std::cout << *info << '\n';
    //     auto instr = br.template read<instr_t>();
    //     std::cout << *instr << '\n';
    // });


    send_recv([](auto& bw) {
        /*
        multipass=0x01 recircs=0x00000000 locks=1,1
        type=REG_1 op=R idx=0x1236 data=0x12345678
        type=REG_3 op=R idx=0x1236 data=0x12345678
        type=REG_4 op=R idx=0x1236 data=0x12345678
        type=REG_6 op=R idx=0x1236 data=0x12345678
        type=REG_26 op=R idx=0x1236 data=0x12345678
        type=REG_28 op=R idx=0x1236 data=0x12345678
        type=REG_38 op=R idx=0x1236 data=0x12345678
        type=REG*1 op=R idx=0x1236 data=0x12345678
        */
        /*auto info =*/ bw.write(info_t{});
        // bw.write(instr_t{InstrType_t::REG(1), OPCode_t::WRITE, 0x1235, 0xaaaaaaab});
        // bw.write(instr_t{InstrType_t::REG(2), OPCode_t::WRITE, 0x1235, 0xaaaaaaab});
        // bw.write(instr_t{InstrType_t::REG(3), OPCode_t::READ, 0x1235, 0x00000000});
        // bw.write(instr_t{InstrType_t::REG(4), OPCode_t::READ, 0x1235, 0x00000000});
        // bw.write(instr_t{InstrType_t::REG(5), OPCode_t::READ, 0x1235, 0x00000000});
        // bw.write(instr_t{InstrType_t::REG(6), OPCode_t::READ, 0x1235, 0x00000000});
        // bw.write(instr_t{InstrType_t::REG(28), OPCode_t::READ, 0x1235, 0x00000000});
        bw.write(instr_t{InstrType_t::REG(29), OPCode_t::READ, 0x1235, 0x00000000});
        // bw.write(instr_t{InstrType_t::REG(20), OPCode_t::READ, 0x1235, 0x00000000});
        // bw.write(instr_t{InstrType_t::REG(1).set_stop(), OPCode_t::READ, 0x1235, 0x00000000});

        // 27+39 works, 28+39 not
        bw.write(InstrType_t::STOP());

        // info->multipass = 1;
        // info->locks = lock_t{1, 1};

        {
            BufferReader br{bw.buffer};
            auto info = br.template read<info_t>();
            std::cout << *info << '\n';
            for (auto i = 0; i < 1; ++i) {        
                auto instr = br.template read<instr_t>();
                std::cout << *instr << '\n';
            }
        } }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        for (auto i = 0; i < 8; ++i) {        
            auto instr = br.template read<instr_t>();
            std::cout << *instr << '\n';
        } });


    return 0;
}