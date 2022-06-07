#include "ycsb.hpp"

#include "db/config.hpp"
#include "random.hpp"
#include "transaction.hpp"

#include <set>
#include <thread>
#include <utility>
#include <vector>


namespace benchmark {
namespace ycsb {

void ycsb_worker(int id, Database& db, TxnExecutorStats& stats) {
    auto& config = Config::instance();

    YCSBRandom rnd(config.node_id << 16 | id);

    YCSBTableInfo ti;
    ti.link_tables(db);


    std::vector<YCSB::Arg_t> txns;
    txns.reserve(config.num_txns);
    for (size_t i = 0; i < config.num_txns; ++i) {
        bool is_hot_txn = rnd.is_hot_txn();

        auto get_txn = [&]() -> YCSB::Arg_t {
            if (rnd.is_multi()) {
                YCSB::Multi<NUM_OPS> txn;
                std::set<uint64_t> ids;
                txn.on_switch = config.use_switch && is_hot_txn;
                txn.is_hot = is_hot_txn;

                bool is_write = rnd.is_write();

                for (int i = 0; i < NUM_OPS; ++i) {
                    auto& op = txn.ops[i];

                    op.id = is_hot_txn ? rnd.hot_id() : rnd.cold_id();
                    if (ids.count(op.id)) { // we only want unique
                        --i;
                        continue;
                    }

                    if constexpr (YCSB_MULTI_MIX_RW) {
                        is_write = rnd.is_write(); // txn can have mixed read_write
                    }

                    if (is_write) {
                        op.mode = AccessMode::WRITE;
                        op.value = rnd.value<uint32_t>();
                    } else {
                        op.mode = AccessMode::READ;
                    }
                    ids.emplace(op.id);
                }


                if constexpr (YCSB_SORT_ACCESSES) {
                    // sort by id
                    // std::sort(txn.ops.begin(), txn.ops.end(), [](const auto& a, const auto& b) {
                    //     return a.id < b.id;
                    // });

                    // sort local first, rest random
                    auto is_local = [&](auto lock) {
                        auto loc_info = ti.kvs->part_info.location(p4db::key_t{lock.id});
                        return loc_info.is_local;
                    };
                    auto& locks = txn.ops;
                    std::sort(locks.begin(), locks.end(), [&](const auto& a, const auto& b) {
                        bool a_local = is_local(a);
                        bool b_local = is_local(b);
                        if (a_local && b_local) {
                            return a.id < b.id;
                        } else if (a_local) {
                            return true;
                        } else if (b_local) {
                            return false;
                        } else {
                            return a.id < b.id;
                        }
                    });
                    // shuffle remote locks
                    auto it = std::find_if_not(locks.begin(), locks.end(), is_local);
                    std::shuffle(it, locks.end(), rnd.gen);
                }

                return txn;
            } else {
                uint64_t id = is_hot_txn ? rnd.hot_id() : rnd.cold_id();
                if (rnd.is_write()) {
                    YCSB::Write txn;
                    txn.id = id;
                    txn.value = rnd.value<uint32_t>();
                    txn.on_switch = config.use_switch && is_hot_txn;
                    txn.is_hot = is_hot_txn;
                    return txn;
                } else {
                    YCSB::Read txn;
                    txn.id = id;
                    txn.on_switch = config.use_switch && is_hot_txn;
                    txn.is_hot = is_hot_txn;
                    return txn;
                }
            }
        };

        auto txn = get_txn();
        txns.push_back(txn);
    }
    // std::random_shuffle(txns.begin(), txns.end());

    db.msg_handler->barrier.wait_workers();

    stats = txn_executor<YCSB>(db, txns);
    stats.count_on_switch(txns);
}


int ycsb() {
    auto& config = Config::instance();
    Database db;

    {
        auto table = db.make_table<StructTable<YCSBTableInfo::KV>>(YCSBTableInfo::KV::TABLE_NAME, config.ycsb.table_size);
        for (uint64_t i = 0; i < config.ycsb.table_size; i++) {
            p4db::key_t index;
            auto& tuple = table->insert(index);
            tuple.id = i;
        }
    }

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
            ycsb_worker(i, db, stat);
        }));
    }

    for (auto& w : workers) {
        w.join();
    }
    std::cout << TxnExecutorStats::accumulate(stats) << '\n';
    db.msg_handler->barrier.wait_nodes();

    // db["kvs"]->print();

    return 0;
}


} // namespace ycsb
} // namespace benchmark