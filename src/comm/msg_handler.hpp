#pragma once

#include "comm/comm.hpp"
#include "comm/msg.hpp"
#include "datastructures/array_hashmap.hpp"
#include "datastructures/stupid_hashmap.hpp"
#include "db/config.hpp"
#include "db/defs.hpp"
#include "db/errors.hpp"
#include "db/future.hpp"
#include "handlers/barrier.hpp"
#include "handlers/init.hpp"
#include "handlers/tuple_put_res.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <vector>


struct Database;

struct MessageHandler {
    using Pkt_t = Communicator::Pkt_t;
    static constexpr auto NUM_FUTURES = 1024;

    Database& db;
    Communicator* comm;
    uint32_t tid;


    InitHandler init;
    BarrierHandler barrier;
    TuplePutResHandler putresponses;


    std::atomic<msg::id_t::type> next_id{0};
    // ArrayHashMap<msg::id_t, AbstractFuture*, NUM_FUTURES> open_futures;    // HINT Not 100% threadsafe
    StupidHashMap<msg::id_t, AbstractFuture*, NUM_FUTURES> open_futures;


    MessageHandler(Database& db, Communicator* comm);
    MessageHandler(MessageHandler&&) = default;
    MessageHandler(const MessageHandler&) = delete;


    msg::id_t set_new_id(msg::Header* msg);

    void add_future(msg::id_t msg_id, AbstractFuture* future);


    void handle(Pkt_t* pkt);

private:
    void handle(Pkt_t* pkt, msg::Init* msg);
    void handle(Pkt_t* pkt, msg::Barrier* msg);

    void handle(Pkt_t* pkt, msg::TupleGetReq* req);
    void handle(Pkt_t* pkt, msg::TupleGetRes* res);
    void handle(Pkt_t* pkt, msg::TuplePutReq* req);
    void handle(Pkt_t* pkt, msg::TuplePutRes* res);

    void handle(Pkt_t* pkt, msg::SwitchTxn* txn);
};