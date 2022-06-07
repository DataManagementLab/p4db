#include "micro_recirc.hpp"

#include "db/config.hpp"
#include "random.hpp"
#include "transaction.hpp"

#include <algorithm>
#include <thread>
#include <vector>

namespace benchmark {
namespace micro_recirc {


void micro_recirc_worker(int id, Database& db, TxnExecutorStats& stats) {
    auto& config = Config::instance();

    MicroRecircRandom rnd(config.node_id << 16 | id);

    std::vector<MicroRecirc::Arg_t> txns;
    txns.reserve(config.num_txns);
    for (size_t i = 0; i < config.num_txns; ++i) {

        MicroRecircArgs::Arg txn;
        txn.recircs = rnd.is_multipass();

        txns.push_back(txn);
    }

    db.msg_handler->barrier.wait_workers();

    stats = txn_executor<MicroRecirc>(db, txns);
    stats.count_on_switch(txns);
}


int micro_recirc() {
    auto& config = Config::instance();

    Database db;

    db.msg_handler->barrier.wait_nodes();

    std::vector<std::thread> workers;
    workers.reserve(config.num_txn_workers);
    std::vector<TxnExecutorStats> stats;
    stats.reserve(config.num_txn_workers);

    for (uint32_t i = 0; i < config.num_txn_workers; ++i) {
        auto& stat = stats.emplace_back();
        workers.emplace_back(std::thread([&, i]() {
            const WorkerContext::guard worker_ctx;
            pin_worker(i);
            micro_recirc_worker(i, db, stat);
        }));
    }

    for (auto& w : workers) {
        w.join();
    }
    std::cout << TxnExecutorStats::accumulate(stats) << '\n';
    db.msg_handler->barrier.wait_nodes();

    return 0;
}


} // namespace micro_recirc
} // namespace benchmark