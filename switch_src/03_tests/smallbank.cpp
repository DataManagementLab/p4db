#include "benchmarks/smallbank/switch.hpp"
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

    using namespace benchmark::smallbank;


    send_recv([](auto& bw) {
        auto info = bw.write(info_t{});
        info->multipass = 0;
        bw.write(balance_t{InstrType_t::BALANCE(0), 0x1234, 0, 0});
        bw.write(InstrType_t::STOP()); }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        auto balance = br.template read<balance_t>();
        std::cout << *balance << '\n'; });


    send_recv([](auto& bw) {
        auto info = bw.write(info_t{});
        info->multipass = 0;
        bw.write(deposit_checking_t{InstrType_t::DEPOSIT_CHECKING(0), 0x1234, -4});
        bw.write(InstrType_t::STOP()); }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        auto deposit = br.template read<deposit_checking_t>();
        std::cout << *deposit << '\n'; });


    send_recv([](auto& bw) {
        auto info = bw.write(info_t{});
        info->multipass = 0;
        bw.write(transact_saving_t{InstrType_t::TRANSACT_SAVING(0), 0x1234, -4});
        bw.write(InstrType_t::STOP()); }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        auto transact = br.template read<transact_saving_t>();
        std::cout << *transact << '\n'; });


    send_recv([](auto& bw) {
        uint8_t P = 1;
        auto info = bw.write(info_t{});
        info->multipass = 1;
        info->locks = lock_t{1, 1};
        bw.write(balance_t{InstrType_t::BALANCE(P), 0x1234, 0, 0});
        bw.write(write_check_egress_t{InstrType_t::WRITE_CHECK_EGRESS().set_stop(true), 0x1234, 0x01});
        bw.write(InstrType_t::STOP()); }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        auto deposit = br.template read<deposit_checking_t>();
        std::cout << *deposit << '\n'; });


    send_recv([](auto& bw) {
        uint8_t P0 = 0, P1 = 0;
        auto info = bw.write(info_t{});
        info->multipass = 1;
        info->locks = lock_t{1, 1};
        bw.write(amalgamate_t{InstrType_t::AMALGAMATE(P0), 0x1234, 0, 0});
        bw.write(amalgamate_egress_t{InstrType_t::AMALGAMATE_EGRESS().set_stop(true), InstrType_t::DEPOSIT_CHECKING(P1), 0x1234});
        bw.write(InstrType_t::STOP()); }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        auto deposit = br.template read<deposit_checking_t>();
        std::cout << *deposit << '\n'; });


    send_recv([](auto& bw) {
        uint8_t P0 = 0, P1 = 0;
        auto info = bw.write(info_t{});
        info->multipass = 1;
        info->locks = lock_t{1, 1};
        bw.write(deposit_checking_t{InstrType_t::DEPOSIT_CHECKING(P0), 0x1235, 0});    // deposit 0 returns current value
        auto c_type_0 = InstrType_t::DEPOSIT_CHECKING(P0);
        auto c_type_1 = InstrType_t::DEPOSIT_CHECKING(P1);
        bw.write(send_payment_egress_t{InstrType_t::SEND_PAYMENT_EGRESS().set_stop(true), c_type_0, c_type_1, 0x1234, 0x20});    // aborts if (x1 < V)
        bw.write(InstrType_t::STOP()); }, [](auto& br) {
        auto info = br.template read<info_t>();
        std::cout << *info << '\n';
        if (*(br.template peek<InstrType_t>()) == InstrType_t::ABORT()) {
            std::cout << "ABORT\n";
            return;
        }
        auto deposit = br.template read<deposit_checking_t>();
        std::cout << *deposit << '\n';
        auto deposit2 = br.template read<deposit_checking_t>();
        std::cout << *deposit2 << '\n'; });

    return 0;
}