#include "msg_handler.hpp"

#include "db/config.hpp"
#include "db/database.hpp"


MessageHandler::MessageHandler(Database& db, Communicator* comm)
    : db(db), comm(comm), tid(comm->mh_tid), init(comm), barrier(comm) {
    comm->set_handler(this);
}


msg::id_t MessageHandler::set_new_id(msg::Header* msg) {
    return msg->msg_id = msg::id_t{next_id.fetch_add(1)};
}

void MessageHandler::add_future(msg::id_t msg_id, AbstractFuture* future) {
    open_futures.insert(msg_id, future);
}


void MessageHandler::handle(Pkt_t* pkt) {
    using namespace msg;

    auto msg = pkt->as<msg::Header>();

    if constexpr (error::DUMP_SWITCH_PKTS) {
        std::cout << "Received:\n";
        pkt->dump(std::cout);
    }

    switch (msg->type) {
        case Type::INIT:
            return handle(pkt, msg->as<msg::Init>());
        case Type::BARRIER:
            return handle(pkt, msg->as<msg::Barrier>());

        case Type::TUPLE_GET_REQ:
            return handle(pkt, msg->as<msg::TupleGetReq>());
        case Type::TUPLE_GET_RES:
            return handle(pkt, msg->as<msg::TupleGetRes>());
        case Type::TUPLE_PUT_REQ:
            return handle(pkt, msg->as<msg::TuplePutReq>());
        case Type::TUPLE_PUT_RES:
            return handle(pkt, msg->as<msg::TuplePutRes>());

        case Type::SWITCH_TXN:
            return handle(pkt, msg->as<msg::SwitchTxn>());
    }
}


// Private methods

void MessageHandler::handle(Pkt_t* pkt, msg::Init* msg) {
    std::cout << "Received msg::Init from " << msg->sender << '\n';
    init.handle(msg->sender);
    pkt->free();
}

void MessageHandler::handle(Pkt_t* pkt, msg::Barrier* msg) {
    std::cout << "Received msg::Barrier from " << msg->sender << '\n';
    barrier.handle();
    pkt->free();
}

void MessageHandler::handle(Pkt_t* pkt, msg::TupleGetReq* req) {
    // std::cerr << "msg::TupleGetReq tid=" << req->tid << " rid=" << req->rid << " mode=" << static_cast<int>(req->mode) << '\n';

    auto table = db[req->tid];
    table->remote_get(pkt, req);
}

void MessageHandler::handle(Pkt_t* pkt, msg::TupleGetRes* res) {
    // std::cerr << "msg::TupleGetRes tid=" << res->tid << " rid=" << res->rid << " mode=" << static_cast<int>(res->mode) << '\n';

    try {
        auto future = open_futures.erase(res->msg_id);
        future->set_pkt(pkt);
    } catch (...) {
        std::cerr << "Received msg_id=" << res->msg_id << " without future.\n";
        pkt->free();
        throw;
    }
    // don't cleanup message buffer, will be cleaned up in undo-log
}

void MessageHandler::handle(Pkt_t* pkt, msg::TuplePutReq* req) {
    // std::cerr << "msg::TuplePutReq tid=" << req->tid << " rid=" << req->rid << " mode=" << static_cast<int>(req->mode) << '\n';
    auto table = db[req->tid];
    table->remote_put(req);

    auto res = req->convert<msg::TuplePutRes>();
    pkt->resize(res->size());
    comm->send(res->sender, pkt, tid);
}

void MessageHandler::handle(Pkt_t* pkt, msg::TuplePutRes* res) {
    // std::cerr << "msg::TuplePutRes tid=" << res->tid << " rid=" << res->rid << " mode=" << static_cast<int>(res->mode) << '\n';

    if (res->mode == AccessMode::INVALID) [[unlikely]] {
        std::cerr << "Received TuplePutRes with invalid AccessMode.\n";
    }

    putresponses.handle(res->sender);
    pkt->free();
}

void MessageHandler::handle(Pkt_t* pkt, msg::SwitchTxn* txn) {
    // std::cerr << "msg::SwitchTxn msg_id=" << txn->msg_id << '\n';
    if constexpr (error::DUMP_SWITCH_PKTS) {
        std::cout << "Handling SwitchTxn len=" << pkt->size() << '\n';
        pkt->dump(std::cerr);
    }

    try {
        auto future = open_futures.erase(txn->msg_id);
        future->set_pkt(pkt);
    } catch (...) {
        std::cerr << "Received msg_id=" << txn->msg_id << " without future.\n";
        pkt->free();
        throw;
    }
}
