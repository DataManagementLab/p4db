#pragma once

#include "comm/comm.hpp"
#include "comm/msg_handler.hpp"
#include "db/future.hpp"
#include "db/mempools.hpp"
#include "stats/context.hpp"


struct Action {
    virtual ~Action() = default;
    virtual void clear(Communicator* comm, uint32_t tid, const timestamp_t ts) = 0;
};

template <typename Table_t, typename Future_t>
struct Write final : public Action {
    Table_t* table;
    p4db::key_t index;
    Future_t* future;

    Write(Table_t* table, p4db::key_t index, Future_t* future)
        : table(table), index(index), future(future) {}

    void clear(Communicator*, uint32_t, const timestamp_t ts) override {
        if (!future->tuple) {
            throw std::runtime_error("tuple not set in undolog clear");
        }
        if (!table->put(index, AccessMode::WRITE, ts)) {
            throw error::UndoException();
        }
    }
};

template <typename Table_t, typename Future_t>
struct Read final : public Action {
    Table_t* table;
    p4db::key_t index;
    Future_t* future;

    Read(Table_t* table, p4db::key_t index, Future_t* future)
        : table(table), index(index), future(future) {}

    void clear(Communicator*, uint32_t, const timestamp_t ts) override {
        if (!future->tuple) {
            throw std::runtime_error("tuple not set in undolog clear");
        }
        if (!table->put(index, AccessMode::READ, ts)) {
            throw error::UndoException();
        }
    }
};

template <typename Tuple_t, AccessMode mode>
struct Remote final : public Action {
    TupleFuture<Tuple_t>* future;
    msg::node_t target;

    Remote(TupleFuture<Tuple_t>* future, msg::node_t target)
        : future(future), target(target) {}

    void clear(Communicator* comm, uint32_t tid, const timestamp_t) override {
        auto tuple = future->get();
        if (!tuple) {
            return; // TupleGetReq Failed, but action is in Log, pkt is already freed earlier
        }

        auto pkt = future->get_pkt();
        auto res = pkt->template as<msg::TupleGetRes>();

        auto req = res->template convert<msg::TuplePutReq>();
        if (req->tid > 4) {
            std::cout << "tid=" << req->tid << " mode=" << static_cast<int>(res->mode) << " msg_id=" << res->msg_id << " tid=" << res->tid << '\n';
            throw std::runtime_error("sth wrong");
        }
        req->sender = msg::node_t{comm->node_id, tid};

        switch (mode) {
            case AccessMode::READ: {
                auto size = msg::TuplePutReq::size(0);
                pkt->resize(size);
                break;
            }
            case AccessMode::WRITE: {
                // size is already enough because we received the tuple of same size
                std::memcpy(req->tuple, tuple, sizeof(Tuple_t));
                break;
            }
            default:
                throw error::InvalidAccessMode();
        }

        comm->handler->putresponses.add(tid);
        comm->send(target, pkt, tid);
    }
};


struct Undolog {
    StackPool<65536> pool;
    std::vector<Action*> actions; // maybe embed into struct
    Communicator* comm;
    uint32_t tid;

    Undolog(Communicator* comm)
        : comm(comm), tid(WorkerContext::get().tid) {}

    template <typename Table_t, typename Future_t>
    void add_write(Table_t* table, p4db::key_t index, Future_t* future) {
        auto action = pool.allocate<Write<Table_t, Future_t>>(table, index, future);
        actions.emplace_back(action);
    }

    template <typename Table_t, typename Future_t>
    void add_read(Table_t* table, p4db::key_t index, Future_t* future) {
        auto action = pool.allocate<Read<Table_t, Future_t>>(table, index, future);
        actions.emplace_back(action);
    }

    template <typename Tuple_t>
    void add_remote_read(TupleFuture<Tuple_t>* future, msg::node_t target) {
        auto action = pool.allocate<Remote<Tuple_t, AccessMode::READ>>(future, target);
        actions.emplace_back(action);
    }

    template <typename Tuple_t>
    void add_remote_write(TupleFuture<Tuple_t>* future, msg::node_t target) {
        auto action = pool.allocate<Remote<Tuple_t, AccessMode::WRITE>>(future, target);
        actions.emplace_back(action);
    }

    void commit(const timestamp_t ts) {
        clear(ts);
    }

    void rollback(const timestamp_t ts) {
        clear(ts);
    }

    void commit_last_n(const timestamp_t ts, const size_t n) {
        clear_last_n(ts, n);
    }

    void rollback_last_n(const timestamp_t ts, const size_t n) {
        clear_last_n(ts, n);
    }

private:
    void clear(const timestamp_t ts);

    void clear_last_n(const timestamp_t ts, const size_t n);
};