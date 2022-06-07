#include "benchmarks/tpcc/switch.hpp"
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

    using namespace benchmark::tpcc;


    // send_recv([](auto& bw) {
    //     auto info = bw.template add<info_t>();
    //     info->multipass = 0;
    //     bw.write(payment_t{0x01, 0x01, 0x1234});
    //     bw.write(InstrType_t::STOP());
    // }, [](auto& br) {
    //     auto info = br.template read<info_t>();
    //     std::cout << *info << '\n';
    //     auto payment = br.template read<payment_t>();
    //     std::cout << "w_id=" << payment->w_id << " d_id=" << payment->d_id << " h_amount=" << payment->h_amount << '\n';
    // });


    send_recv([](auto& bw) {
        auto info = bw.write(info_t{});

        // for (uint64_t i = 0; i < arg.args.ol_cnt; ++i) {
        //     bw.write(no_stock_t{InstrType_t::NO_STOCK(0).set_stop(i > 0), arg.get_s_id(), arg.get_ol_quantity(), be_uint8_t{1}});
        // }
        bw.write(no_stock_t{InstrType_t::NO_STOCK(0), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(1), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(2), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(3), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(4), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(5), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(6), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(7), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(8), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(9), 0x0001, 0x00000001, be_uint8_t{0}}); // 10

        bw.write(no_stock_t{InstrType_t::NO_STOCK(0).set_stop(1), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(1), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(2), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(3), 0x0001, 0x00000001, be_uint8_t{0}});
        bw.write(no_stock_t{InstrType_t::NO_STOCK(4), 0x0001, 0x00000001, be_uint8_t{0}});
        /*auto order =*/ bw.write(new_order_t{0x0001}); // always last!
        bw.write(InstrType_t::STOP());

        info->multipass = 1;
        info->locks = lock_t{1, 1}; },
              [](auto& br) {
                  auto info = br.template read<info_t>();
                  std::cout << *info << '\n';
                  for (int i = 0; i < 15; i++) {
                      br.template read<no_stock_t>();
                  }
                  auto new_order = br.template read<new_order_t>();
                  std::cout << "d_id=" << new_order->d_id << " d_next_o_id=" << new_order->d_next_o_id << '\n';
              });


    return 0;
}