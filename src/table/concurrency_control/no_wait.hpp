#pragma once


#include "db/future.hpp"
#include "db/spinlock.hpp"
#include "db/types.hpp"
#include "row.hpp"
#include "stats/stats.hpp"

#include <mutex>
#include <tbb/queuing_mutex.h>
#include <tbb/queuing_rw_mutex.h>


template <typename Tuple_t>
struct Row<Tuple_t, CC_Scheme::NO_WAIT> {
    using lock_t = std::mutex;
    // using lock_t = SpinLock;
    // using lock_t = tbb::queuing_mutex;

    lock_t mutex;

    AccessMode lock_type = AccessMode::INVALID;
    uint32_t owner_cnt = 0;

    Tuple_t tuple;

    using Future_t = TupleFuture<Tuple_t>;


    ErrorCode local_lock(const AccessMode mode, timestamp_t, Future_t* future) {
        if (!is_compatible(mode)) { // early abort test
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

        WorkerContext::get().cycl.start(stats::Cycles::latch_contention);
        const std::lock_guard<lock_t> lock(mutex);
        WorkerContext::get().cycl.stop(stats::Cycles::latch_contention);
        // lock_t::scoped_lock lock;
        // lock.acquire(mutex);

        if (!is_compatible(mode)) {
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

        ++owner_cnt;
        lock_type = mode;
        future->tuple.store(&tuple);

        WorkerContext::get().cntr.incr(stats::Counter::local_lock_success);
        return ErrorCode::SUCCESS;
    }

    void remote_lock(Communicator& comm, Communicator::Pkt_t* pkt, msg::TupleGetReq* req) {
        WorkerContext::get().cycl.start(stats::Cycles::latch_contention);
        const std::lock_guard<lock_t> lock(mutex);
        WorkerContext::get().cycl.stop(stats::Cycles::latch_contention);
        // lock_t::scoped_lock lock;
        // lock.acquire(mutex);


        if (!is_compatible(req->mode)) {
            auto res = req->convert<msg::TupleGetRes>();
            res->mode = AccessMode::INVALID;
            comm.send(res->sender, pkt, comm.mh_tid); // always called from msg-handler
            WorkerContext::get().cntr.incr(stats::Counter::remote_lock_failed);
            return;
        }

        ++owner_cnt;
        lock_type = req->mode;

        auto res = req->convert<msg::TupleGetRes>();
        auto size = msg::TupleGetRes::size(sizeof(tuple));
        pkt->resize(size);
        std::memcpy(res->tuple, &tuple, sizeof(tuple));

        WorkerContext::get().cntr.incr(stats::Counter::remote_lock_success);
        comm.send(res->sender, pkt, comm.mh_tid); // always called from msg-handler
    }

    void remote_unlock(msg::TuplePutReq* req, Communicator& comm) {
        if (req->mode == AccessMode::WRITE) {
            std::memcpy(&tuple, req->tuple, sizeof(tuple));
        }
        auto rc = local_unlock(req->mode, req->ts, comm);
        (void)rc;
    }

    ErrorCode local_unlock(const AccessMode mode, const timestamp_t, Communicator&) {
        WorkerContext::get().cycl.start(stats::Cycles::latch_contention);
        const std::lock_guard<lock_t> lock(mutex);
        WorkerContext::get().cycl.stop(stats::Cycles::latch_contention);
        // lock_t::scoped_lock lock;
        // lock.acquire(mutex);

        if (lock_type != mode) [[unlikely]] {
            std::cout << "lock_type=" << lock_type << " mode=" << mode << '\n';
            return ErrorCode::INVALID_ACCESS_MODE;
        }

        if (--owner_cnt > 0) {
            return ErrorCode::SUCCESS;
        }
        // owner_cnt == 0 -> no active locks

        lock_type = AccessMode::INVALID;
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
        return (lock_type == AccessMode::INVALID);
    }
};


// template<typename Tuple_t>
// struct Row<Tuple_t, CC_Scheme::NO_WAIT> {
//     std::shared_mutex lock;
//     Tuple_t tuple;

//     using Future_t = TupleFuture<Tuple_t>;


//     ErrorCode local_lock(const AccessMode mode, timestamp_t, Future_t* future) {
//         switch(mode) {
//             case AccessMode::READ:
//                 if (!lock.try_lock_shared()) {
//                     return ErrorCode::READ_LOCK_FAILED;
//                 }
//                 break;
//             case AccessMode::WRITE:
//                 if (!lock.try_lock()) {
//                     return ErrorCode::WRITE_LOCK_FAILED;
//                 }
//                 break;
//             default:
//                 return ErrorCode::INVALID_ACCESS_MODE;
//         }

//         future->tuple.store(&tuple);
//         return ErrorCode::SUCCESS;
//     }

//     void remote_lock(Communicator& comm, Communicator::Pkt_t* pkt, msg::TupleGetReq* req) {
//         bool failed = false;
//         switch(req->mode) {
//             case AccessMode::READ:
//                 failed = !lock.try_lock_shared();
//                 break;
//             case AccessMode::WRITE:
//                 failed = !lock.try_lock();
//                 break;
//             default:
//                 failed = true;
//         }

//         if (failed) {
//             auto res = req->convert<msg::TupleGetRes>();
//             res->mode = AccessMode::INVALID;
//             comm.send(res->sender, pkt, comm.mh_tid);   // always called from msg-handler
//             return;
//         }


//         auto res = req->convert<msg::TupleGetRes>();
//         auto size = msg::TupleGetRes::size(sizeof(tuple));
//         pkt->resize(size);
//         std::memcpy(res->tuple, &tuple, sizeof(tuple));

//         comm.send(res->sender, pkt, comm.mh_tid);   // always called from msg-handler
//     }

//     void remote_unlock(msg::TuplePutReq* req, Communicator& comm) {
//         if (req->mode == AccessMode::WRITE) {
//             std::memcpy(&tuple, req->tuple, sizeof(tuple));
//         }
//         auto rc = local_unlock(req->mode, comm);
//         (void) rc;
//     }

//     ErrorCode local_unlock(const AccessMode mode, Communicator&) {
//         switch(mode) {
//             case AccessMode::READ:
//                 lock.unlock_shared();
//                 break;
//             case AccessMode::WRITE:
//                 lock.unlock();
//                 break;
//             default:
//                 return ErrorCode::INVALID_ACCESS_MODE;
//         }
//         return ErrorCode::SUCCESS;
//     }

//     bool check() {
//         std::stringstream ss;
//         bool shared = lock.try_lock_shared();
//         if (shared) {
//             lock.unlock_shared();
//         } else {
//             ss << "try_lock_shared() failed\n";
//         }
//         bool exclusive = lock.try_lock();
//         if (exclusive) {
//             lock.unlock();
//         } else {
//             ss << "try_lock() failed\n";
//         }

//         if (!shared || !exclusive) {
//             std::cerr << ss.str();
//             return false;
//         }
//         return true;
//     }
// };
