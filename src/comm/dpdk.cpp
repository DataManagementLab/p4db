#include "dpdk.hpp"

#include "comm/msg_handler.hpp"
#include "db/config.hpp"
#include "stats/stats.hpp"

#include <fmt/core.h>


DPDKCommunicator::DPDKCommunicator() {
    auto& config = Config::instance();
    node_id = config.node_id;
    num_nodes = config.num_nodes;
    switch_id = config.switch_id;


    if (!Dpdk::init({0, 1}, 1024 * 4 * 4 * 4 /*16*/ - 1, 0)) {
        EXIT_WITH_ERROR("couldn't initialize Dpdk");
    }

    auto& dpdk = Dpdk::getInstance();
    auto devices = dpdk.getDpdkDeviceList();
    device = devices.at(0);

    num_rx_queues = 1;
    num_tx_queues = config.num_txn_workers + 1 /* handler */ + 1 /* spin-lock */; // + 1 + config->txn_agents; // 0 with spin-lock , 1 for table-agent, 2++ for txn_agents
    mh_tid = config.num_txn_workers;
    spin_tx_queue = config.num_txn_workers + 1;
    if (!device->openMultiQueues(num_rx_queues, num_tx_queues)) {
        EXIT_WITH_ERROR("Couldn't open Dpdk device #%d, PMD '%s'", device->getDeviceId(), device->getPMDName().c_str());
    }

    MacAddress mac = device->getMacAddress();
    src_mac = eth_addr_t{mac.m_Address[0], mac.m_Address[1], mac.m_Address[2], mac.m_Address[3], mac.m_Address[4], mac.m_Address[5]};

    targets.reserve(config.servers.size());
    for (auto& server : config.servers) {
        targets.emplace_back(server.mac);
    }

    if (src_mac != targets[node_id].mac) {
        throw std::runtime_error("macs do not match");
    }

    stats_thread = std::jthread([&](std::stop_token token) {
        // auto& config = Config::instance();

        // pin_worker(config.num_txn_workers);
        // logger::info("Press ENTER for stats.");
        for (uint64_t i = 1; !token.stop_requested(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;

            printf("Stats #%zu\n==========\n", i);

            DpdkDevice::DpdkDeviceStats stats;
            device->getStatistics(stats);

            std::stringstream ss;
            ss << "\nStatistics for port " << device->getDeviceId() << ":\n";

            ss << fmt::format("{:>10} {:>15} {:>15} {:>15} {:>15} {:>15}\n", " ", "Total Packets", "Packets/sec", "Total Bytes", "Bytes/sec", "Gbps");

            ss << fmt::format("{:10} {:15} {:15} {:15} {:15} {:15.4f}\n", "rx",
                              stats.aggregatedRxStats.packets, stats.aggregatedRxStats.packetsPerSec, stats.aggregatedRxStats.bytes,
                              stats.aggregatedRxStats.bytesPerSec, stats.aggregatedRxStats.bytesPerSec / 1.25e+8);

            ss << fmt::format("{:10} {:15} {:15} {:15} {:15} {:15.4f}\n", "tx",
                              stats.aggregatedTxStats.packets, stats.aggregatedTxStats.packetsPerSec, stats.aggregatedTxStats.bytes,
                              stats.aggregatedTxStats.bytesPerSec, stats.aggregatedTxStats.bytesPerSec / 1.25e+8);


            ss << "getAmountOfFreeMbufs()=" << device->getAmountOfFreeMbufs() << " getAmountOfMbufsInUse()=" << device->getAmountOfMbufsInUse() << '\n';
            ss << "rxPacketsDroppedByHW=" << stats.rxPacketsDroppedByHW << " rxErroneousPackets=" << stats.rxErroneousPackets << " rxMbufAlocFailed=" << stats.rxMbufAlocFailed << " txFailedPackets=" << stats.txFailedPackets << '\n';

            std::cout << ss.str() << std::flush;
        }
    });
}


DPDKCommunicator::~DPDKCommunicator() {
    // if (stats_thread.joinable()) {
    //     stats_thread.join();
    // }
    auto& dpdk = Dpdk::getInstance();
    dpdk.stopDpdkWorkerThreads();
}


void DPDKCommunicator::set_handler(MessageHandler* handler) {
    this->handler = handler;

    auto& dpdk = Dpdk::getInstance();
    dpdk.start_worker<ReceiverThread>(device, num_rx_queues, mh_tid, handler);
}


static constexpr uint16_t ETHER_TYPE = 0x1000;

void DPDKCommunicator::send(msg::node_t target, DPDKPacketBuffer*& pkt) {
    pkt->ctor_eth(targets[target].mac, src_mac, be_uint16_t{ETHER_TYPE});

    if constexpr (error::DUMP_SWITCH_PKTS) {
        std::cout << "Packet to: " << target << '\n';
        pkt->dump(std::cout);
    }

    mutex.lock();
    bool success = device->send(pkt, spin_tx_queue);
    mutex.unlock();

    if (!success) {
        throw std::runtime_error("Failed to send");
    }
    pkt = nullptr;
}

void DPDKCommunicator::send(msg::node_t target, DPDKPacketBuffer*& pkt, uint32_t tid) {
    pkt->ctor_eth(targets[target].mac, src_mac, be_uint16_t{ETHER_TYPE});

    if constexpr (error::DUMP_SWITCH_PKTS) {
        std::cout << "Packet to: " << target << '\n';
        pkt->dump(std::cout);
    }


    uint16_t tx_queue = static_cast<uint16_t>(tid);
    bool success = device->send(pkt, tx_queue);

    if (!success) {
        throw std::runtime_error("Failed to send");
    }
    pkt = nullptr;
}


DPDKPacketBuffer* DPDKCommunicator::make_pkt() {
    return DPDKPacketBuffer::alloc(device.get());
}


bool ReceiverThread::run(uint32_t core_id) {
    const WorkerContext::guard worker_ctx;
    WorkerContext::get().tid = mh_tid;
    this->core_id = core_id;

    DPDKPacket* pkts[MAX_RECEIVE_BURST];

    while (!do_stop) {
        for (uint16_t rx_queue = 0; rx_queue < rx_queues; ++rx_queue) {
            uint16_t nb_pkts = device->receive(pkts, MAX_RECEIVE_BURST, rx_queue);

            for (uint16_t i = 0; i < nb_pkts; ++i) {
                auto pkt = static_cast<DPDKPacketBuffer*>(pkts[i]);
                handler->handle(pkt);
            }
        }
    }
    return true;
}