#pragma once


#include "datastructures/linked_list.hpp"
#include "db/future.hpp"
#include "db/spinlock.hpp"
#include "db/types.hpp"
#include "row.hpp"
#include "stats/stats.hpp"

#include <mutex>


template <typename Tuple_t>
struct Row<Tuple_t, CC_Scheme::WAIT_DIE> {
    struct Waiter {
        AccessMode mode;
        timestamp_t ts;
        bool is_remote;
        union {
            Communicator::Pkt_t* pkt;
            TupleFuture<Tuple_t>* future;
        };

        bool operator<(const Waiter& other) {
            return ts > other.ts;
        }
    };

    struct Owner {
        timestamp_t ts;

        bool operator<(const Owner& other) {
            return ts < other.ts;
        }
    };

    using lock_t = std::mutex;
    // using lock_t = SpinLock;
    lock_t mutex;


    AccessMode lock_type = AccessMode::INVALID;
    uint32_t owner_cnt = 0;

    LinkedList<Waiter> waiters; // is sorted in reverse
    LinkedList<Owner> owners;   // is sorted

    Tuple_t tuple;

    using Future_t = TupleFuture<Tuple_t>;

    void invariant() {
        auto owner = owners.head;
        while (owner) {
            auto waiter = waiters.head;
            while (waiter) {
                if (waiter->val.ts > owner->val.ts) { // waiter can wait if w.ts > o
                    throw std::runtime_error("invariant not given");
                }
                waiter = waiter->next;
            }
            owner = owner->next;
        }
    }

    ErrorCode local_lock(const AccessMode mode, timestamp_t ts, Future_t* future) {
        const std::lock_guard<lock_t> lock(mutex);

        // invariant();

        bool conflict = !is_compatible(mode);
        if (!conflict) {
            conflict = !waiters.empty() && (ts < waiters.head->val.ts);
        }

        if (!conflict) {
            ++owner_cnt;
            lock_type = mode;
            owners.add_sorted(Owner{ts});

            // future->tuple = &tuple;   // SUCCESS
            future->tuple.store(&tuple);
            WorkerContext::get().cntr.incr(stats::Counter::local_lock_success);
            // invariant();
            return ErrorCode::SUCCESS;
        }

        bool can_wait = owners.empty() || (owners.head->val.ts > ts);
        // bool can_wait = true;
        // auto e = owners.head;
        // while (e) {
        //     if (e->val.ts < ts) {
        //         can_wait = false;
        //         break;
        //     }
        //     e = e->next;
        // }
        if (!can_wait) { // FAIL
            // invariant();
            switch (mode) {
                case AccessMode::READ:
                    WorkerContext::get().cntr.incr(stats::Counter::local_read_lock_failed);
                    return ErrorCode::READ_LOCK_FAILED;
                case AccessMode::WRITE:
                    WorkerContext::get().cntr.incr(stats::Counter::local_write_lock_failed);
                    return ErrorCode::WRITE_LOCK_FAILED;
                default:
                    return ErrorCode::INVALID_ACCESS_MODE;
            }
        }


        Waiter entry;
        entry.mode = mode;
        entry.ts = ts;
        entry.is_remote = false;
        entry.future = future; // union

        waiters.add_sorted(std::move(entry));
        WorkerContext::get().cntr.incr(stats::Counter::local_lock_waiting);
        // invariant();
        return ErrorCode::SUCCESS;
    }

    void remote_lock(Communicator& comm, Communicator::Pkt_t* pkt, msg::TupleGetReq* req) {
        const std::lock_guard<lock_t> lock(mutex);

        bool conflict = !is_compatible(req->mode);
        if (!conflict) {
            conflict = !waiters.empty() && (req->ts < waiters.head->val.ts);
        }
        if (!conflict) {
            ++owner_cnt;
            lock_type = req->mode;
            owners.add_sorted(Owner{req->ts});

            auto res = req->convert<msg::TupleGetRes>();
            auto size = msg::TupleGetRes::size(sizeof(tuple));
            pkt->resize(size);
            std::memcpy(res->tuple, &tuple, sizeof(tuple));

            comm.send(res->sender, pkt, comm.mh_tid); // always called from msg-handler
            WorkerContext::get().cntr.incr(stats::Counter::remote_lock_success);
            return;
        }


        bool can_wait = owners.empty() || (owners.head->val.ts > req->ts);
        if (!can_wait) { // FAIL
            auto res = req->convert<msg::TupleGetRes>();
            res->mode = AccessMode::INVALID;
            comm.send(res->sender, pkt, comm.mh_tid); // always called from msg-handler
            WorkerContext::get().cntr.incr(stats::Counter::remote_lock_failed);
            return;
        }


        Waiter entry;
        entry.mode = req->mode;
        entry.ts = req->ts;
        entry.is_remote = true;
        entry.pkt = pkt; // union

        waiters.add_sorted(std::move(entry));
        WorkerContext::get().cntr.incr(stats::Counter::remote_lock_waiting);
    }

    void remote_unlock(msg::TuplePutReq* req, Communicator& comm) {
        if (req->mode == AccessMode::WRITE) {
            std::memcpy(&tuple, req->tuple, sizeof(tuple));
        }
        auto rc = local_unlock(req->mode, req->ts, comm);
        (void)rc;
    }

    ErrorCode local_unlock(const AccessMode mode, const timestamp_t ts, Communicator& comm) {
        const std::lock_guard<lock_t> lock(mutex);

        // invariant();
        if (lock_type != mode) {
            return ErrorCode::INVALID_ACCESS_MODE;
        }

        owners.remove_if_one([&](const auto& entry) {
            return entry.ts == ts;
        });
        --owner_cnt;

        if (owner_cnt > 0) {
            // invariant();
            return ErrorCode::SUCCESS;
        }
        // owner_cnt == 0 -> no active locks
        // max_owner_ts = timestamp_t{0};

        lock_type = AccessMode::INVALID;

        // invariant();
        uint32_t my_tid = WorkerContext::get().tid;
        waiters.remove_until([&](const auto& entry) {
            if (!is_compatible(entry.mode)) {
                return false;
            }
            ++owner_cnt;
            lock_type = entry.mode;
            owners.add_sorted(Owner{entry.ts});
            // max_owner_ts = timestamp_t{std::max(max_owner_ts, entry.ts)};

            if (!entry.is_remote) {
                // entry.future->tuple = &tuple;
                entry.future->tuple.store(&tuple);
                WorkerContext::get().cntr.incr(stats::Counter::local_lock_success);
            } else {
                auto pkt = entry.pkt;
                auto req = pkt->template as<msg::TupleGetReq>();
                auto res = req->template convert<msg::TupleGetRes>();
                auto size = msg::TupleGetRes::size(sizeof(tuple));
                pkt->resize(size);
                std::memcpy(res->tuple, &tuple, sizeof(tuple));

                comm.send(res->sender, pkt, my_tid);
                WorkerContext::get().cntr.incr(stats::Counter::remote_lock_success);
            }

            return true;
        });

        // invariant();
        // done processing all pending open lock-requests
        return ErrorCode::SUCCESS;
    }

    bool is_compatible(AccessMode mode) {
        if (lock_type == AccessMode::INVALID) {
            return true;
        }
        if (lock_type == AccessMode::WRITE || mode == AccessMode::WRITE) {
            return false;
        }
        return true; // shared
    };

    bool check() {
        return waiters.empty() && owners.empty() && (lock_type == AccessMode::INVALID);
    }
};