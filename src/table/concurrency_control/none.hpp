#pragma once


#include "db/future.hpp"
#include "db/types.hpp"
#include "row.hpp"
#include "stats/stats.hpp"

#include <mutex>


template <typename Tuple_t>
struct Row<Tuple_t, CC_Scheme::NONE> {
    Tuple_t tuple;

    using Future_t = TupleFuture<Tuple_t>;


    ErrorCode local_lock(const AccessMode, timestamp_t, Future_t* future) {
        future->tuple.store(&tuple);

        WorkerContext::get().cntr.incr(stats::Counter::local_lock_success);
        return ErrorCode::SUCCESS;
    }

    void remote_lock(Communicator& comm, Communicator::Pkt_t* pkt, msg::TupleGetReq* req) {
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

    ErrorCode local_unlock(const AccessMode, const timestamp_t, Communicator&) {
        return ErrorCode::SUCCESS;
    }

    bool check() {
        return true;
    }
};