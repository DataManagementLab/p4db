#pragma once

#include "comm/msg.hpp"
#include "comm/msg_handler.hpp"
#include "db/buffers.hpp"
#include "db/database.hpp"
#include "db/defs.hpp"
#include "db/errors.hpp"
#include "db/future.hpp"
#include "db/mempools.hpp"
#include "db/ts_factory.hpp"
#include "db/types.hpp"
#include "db/undolog.hpp"
#include "db/util.hpp"
#include "stats/context.hpp"
#include "table/table.hpp"

#include <iostream>
#include <vector>


struct Transaction {
    enum RC {
        COMMIT,
        ROLLBACK,
    };
};


#define check(x)               \
    do {                       \
        if (!x) [[unlikely]] { \
            return rollback(); \
        }                      \
    } while (0)


template <typename T, typename TransactionArgs, typename TableInfo>
struct TransactionBase : crtp<T>, public Transaction, public TransactionArgs, public TableInfo {

    template <typename Tuple_t>
    using Table_t = typename TableInfo::Table_t<Tuple_t>;

    using Arg_t = typename TransactionArgs::Arg_t;

    Database& db;
    Undolog log;
    StackPool<8192> mempool;
    uint32_t tid;

    TimestampFactory ts_factory;
    timestamp_t ts;

    TransactionBase(Database& db)
        : db(db), log(db.comm.get()), tid(WorkerContext::get().tid) {}


    RC execute(Arg_t& arg) {
        ts = ts_factory.get();
        // std::stringstream ss;
        // ss << "Starting txn tid=" << tid << " ts=" << ts << '\n';
        // std::cout << ss.str();

        WorkerContext::get().cycl.reset(stats::Cycles::commit_latency);
        WorkerContext::get().cycl.reset(stats::Cycles::latch_contention);
        WorkerContext::get().cycl.reset(stats::Cycles::remote_latency);
        WorkerContext::get().cycl.reset(stats::Cycles::local_latency);
        WorkerContext::get().cycl.reset(stats::Cycles::switch_txn_latency);

        WorkerContext::get().cycl.start(stats::Cycles::commit_latency);
        return std::visit(this->underlying(), arg);
    }


    RC commit() {
        if constexpr (CC_SCHEME != CC_Scheme::NONE) {
            log.commit(ts);
        }
        mempool.clear();
        WorkerContext::get().cycl.stop(stats::Cycles::commit_latency);

        WorkerContext::get().cycl.save(stats::Cycles::commit_latency);
        WorkerContext::get().cycl.save(stats::Cycles::latch_contention);
        WorkerContext::get().cycl.save(stats::Cycles::remote_latency);
        WorkerContext::get().cycl.save(stats::Cycles::local_latency);
        WorkerContext::get().cycl.save(stats::Cycles::switch_txn_latency);

        WorkerContext::get().pcntr.incr(stats::Periodic::commits);
        return RC::COMMIT;
    }

    RC rollback() {
        if constexpr (CC_SCHEME != CC_Scheme::NONE) {
            log.rollback(ts);
        }
        mempool.clear();

        // for (int i = 0; i < 128; ++i) { // abort backoff
        //     __builtin_ia32_pause();
        // }

        return RC::ROLLBACK;
    }


    template <typename Tuple_t>
    TupleFuture<Tuple_t>* read(Table_t<Tuple_t>* table, p4db::key_t key) {
        using Future_t = TupleFuture<Tuple_t>; // TODO return with const Tuple_t

        auto loc_info = table->part_info.location(key);

        if constexpr (error::LOG_TABLE) {
            std::stringstream ss;
            ss << "read to " << table->name << " key=" << key << " is_local=" << loc_info.is_local
               << " target=" << loc_info.target << " is_hot=" << loc_info.is_hot << " switch_idx=" << loc_info.abs_hot_index << '\n';
            std::cout << ss.str();
        }

        if (loc_info.is_local) {
            WorkerContext::get().cycl.start(stats::Cycles::local_latency);
            auto future = mempool.allocate<Future_t>();
            if (!table->get(key, AccessMode::READ, future, ts)) [[unlikely]] {
                return nullptr;
            }
            if constexpr (CC_SCHEME != CC_Scheme::NONE) {
                log.add_read(table, key, future); // TODO passing future necessary?
            }
            if (!future->get()) [[unlikely]] {
                return nullptr;
            } // make optional for NO_WAIT
            WorkerContext::get().cycl.stop(stats::Cycles::local_latency);
            return future;
        }

        AccessMode mode = AccessMode::READ;
        if constexpr (LM_ON_SWITCH) {
            if (loc_info.is_hot) {
                mode.set_switch_index(loc_info.abs_hot_index); // converts to big-endian
            }
        }
        WorkerContext::get().cycl.start(stats::Cycles::remote_latency);
        auto pkt = db.comm->make_pkt();
        auto req = pkt->ctor<msg::TupleGetReq>(ts, table->id, key, mode);
        req->sender = db.comm->node_id;

        auto future = mempool.allocate<Future_t>();
        auto msg_id = db.msg_handler->set_new_id(req);
        db.msg_handler->add_future(msg_id, future);

        db.comm->send(loc_info.target, pkt, tid);
        if constexpr (CC_SCHEME != CC_Scheme::NONE) {
            log.add_remote_read(future, loc_info.target);
        }
        if (!future->get()) [[unlikely]] {
            return nullptr;
        }
        WorkerContext::get().cycl.stop(stats::Cycles::remote_latency);
        return future;
    }

    template <typename Tuple_t>
    TupleFuture<Tuple_t>* write(Table_t<Tuple_t>* table, p4db::key_t key) {
        using Future_t = TupleFuture<Tuple_t>;

        auto loc_info = table->part_info.location(key);

        if constexpr (error::LOG_TABLE) {
            std::stringstream ss;
            ss << "write to " << table->name << " key=" << key << " is_local=" << loc_info.is_local
               << " target=" << loc_info.target << " is_hot=" << loc_info.is_hot << " switch_idx=" << loc_info.abs_hot_index << '\n';
            std::cout << ss.str();
        }

        if (loc_info.is_local) {
            WorkerContext::get().cycl.start(stats::Cycles::local_latency);
            auto future = mempool.allocate<Future_t>();
            future->tuple = nullptr;
            if (!table->get(key, AccessMode::WRITE, future, ts)) [[unlikely]] {
                return nullptr;
            }
            if constexpr (CC_SCHEME != CC_Scheme::NONE) {
                log.add_write(table, key, future);
            }
            if (!future->get()) [[unlikely]] {
                return nullptr;
            }
            WorkerContext::get().cycl.stop(stats::Cycles::local_latency);
            return future;
        }

        AccessMode mode = AccessMode::WRITE;
        if constexpr (LM_ON_SWITCH) {
            if (loc_info.is_hot) {
                mode.set_switch_index(loc_info.abs_hot_index); // converts to big-endian
            }
        }

        WorkerContext::get().cycl.start(stats::Cycles::remote_latency);
        auto pkt = db.comm->make_pkt();
        auto req = pkt->ctor<msg::TupleGetReq>(ts, table->id, key, mode);
        req->sender = db.comm->node_id;

        auto future = mempool.allocate<Future_t>();
        auto msg_id = db.msg_handler->set_new_id(req);
        db.msg_handler->add_future(msg_id, future);

        db.comm->send(loc_info.target, pkt, tid);
        if constexpr (CC_SCHEME != CC_Scheme::NONE) {
            log.add_remote_write(future, loc_info.target);
        }
        if (!future->get()) [[unlikely]] {
            return nullptr;
        }
        WorkerContext::get().cycl.stop(stats::Cycles::remote_latency);
        return future;
    }

    template <typename Tuple_t>
    TupleFuture<Tuple_t>* insert(Table_t<Tuple_t>* table) {
        WorkerContext::get().cycl.start(stats::Cycles::local_latency);
        using Future_t = TupleFuture<Tuple_t>;
        p4db::key_t key;
        table->insert(key);

        auto future = mempool.allocate<Future_t>();
        if (!table->get(key, AccessMode::WRITE, future, ts)) [[unlikely]] {
            return nullptr;
        }
        if constexpr (CC_SCHEME != CC_Scheme::NONE) {
            log.add_write(table, key, future);
        }
        if (!future->get()) [[unlikely]] {
            return nullptr;
        }
        WorkerContext::get().cycl.stop(stats::Cycles::local_latency);
        return future;
    }


    template <typename P4Switch, typename Arg_t>
    auto atomic(P4Switch& p4_switch, const Arg_t& arg) {
        auto& comm = db.comm;

        auto pkt = comm->make_pkt();
        auto txn = pkt->ctor<msg::SwitchTxn>();
        txn->sender = comm->node_id;

        BufferWriter bw{txn->data};
        p4_switch.make_txn(arg, bw);

        auto size = msg::SwitchTxn::size(bw.size);
        pkt->resize(size);

        auto parse_fn = [&](Communicator::Pkt_t* pkt) {
            auto txn = pkt->as<msg::SwitchTxn>();
            BufferReader br{txn->data};
            return p4_switch.parse_txn(arg, br);
        };
        using Future_t = SwitchFuture<decltype(parse_fn)>;

        auto future = mempool.allocate<Future_t>(std::move(parse_fn));
        auto msg_id = comm->handler->set_new_id(txn);
        comm->handler->add_future(msg_id, future);
        comm->send(comm->switch_id, pkt, tid);

        return future;
    }
};


struct TxnExecutorStats { // TODO merge with counters?
    uint64_t commits = 0;
    uint64_t rollbacks = 0;
    uint64_t num_txns = 0;
    int64_t duration = 0;
    uint64_t on_switch = 0;

    template <typename T>
    static auto accumulate(T& container) {
        TxnExecutorStats stats;
        for (auto& s : container) {
            stats.commits += s.commits;
            stats.rollbacks += s.rollbacks;
            stats.num_txns += s.num_txns;
            stats.duration += s.duration;
            stats.on_switch += s.on_switch;
        }
        if (container.size() > 0) {
            stats.duration /= container.size();
        }
        return stats;
    }

    template <typename T>
    void count_on_switch(T& txns) {
        on_switch = std::count_if(txns.begin(), txns.end(), [](auto& txn) {
            return std::visit([](auto& txn) {
                return txn.on_switch;
            },
                              txn);
        });
    }

    friend std::ostream& operator<<(std::ostream& os, const TxnExecutorStats& self) {
        auto total_tps = (self.num_txns > 0) ? static_cast<uint64_t>(self.num_txns / (self.duration / 1e6)) : 0;
        auto total_cps = (self.commits > 0) ? static_cast<uint64_t>(self.commits / (self.duration / 1e6)) : 0;

        std::stringstream ss;
        ss << "*** Summary ***\n";
        ss << "total_tps=" << total_tps << " txns/s\n";
        ss << "total_cps=" << total_cps << " commits/s\n";
        ss << "total_commits=" << self.commits << '\n';
        ss << "total_aborts=" << self.rollbacks << '\n';
        ss << "total_txns=" << self.num_txns << '\n';
        ss << "total_on_switch=" << self.on_switch << '\n';
        ss << "avg_duration=" << self.duration << " µs\n";

        os << ss.str();
        return os;
    }
};


template <typename Transaction_t, typename Arg_t>
auto txn_executor(Database& db, std::vector<Arg_t> txns) {

    TxnExecutorStats stats;

    Transaction_t txn{db};

    auto start = std::chrono::high_resolution_clock::now();

    for (auto& arg : txns) {
        auto rc = txn.execute(arg);

        // std::stringstream ss;
        // ss << "Finished txn tid=" << txn.tid << " ts=" << txn.ts << " rc=" << rc << '\n';
        // std::cout << ss.str();

        switch (rc) {
            case Transaction::COMMIT:
                ++stats.commits;
                break;
            case Transaction::ROLLBACK:
                // std::cout << "rollback ts=" << txn.ts << '\n';
                ++stats.rollbacks;
                break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    stats.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    stats.num_txns = txns.size();

    auto tps = (stats.num_txns > 0) ? static_cast<uint64_t>(stats.num_txns / (stats.duration / 1e6)) : 0;
    std::stringstream ss;
    ss << "Worker " << WorkerContext::get().tid << " took: " << stats.duration << "µs --> " << tps << " txns/sec\n";
    ss << "commits=" << stats.commits << " aborts=" << stats.rollbacks << '\n';
    std::cout << ss.str();


    return stats;
}