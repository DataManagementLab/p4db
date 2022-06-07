#pragma once


#include "comm/comm.hpp"
#include "db/errors.hpp"

#include <atomic>
#include <limits>


struct AbstractFuture {
    std::atomic<Communicator::Pkt_t*> pkt{nullptr}; // msg::TupleGetRes

    void set_pkt(Communicator::Pkt_t* pkt) {
        this->pkt.store(pkt, std::memory_order_release);
    }

    auto get_pkt() {
        Communicator::Pkt_t* pkt;
        // Wait for pkt without generating cache misses
        while (!(pkt = this->pkt.load(std::memory_order_relaxed))) {
            __builtin_ia32_pause();
        }
        return pkt;
    }
};


template <typename Tuple_t>
struct TupleFuture final : public AbstractFuture {
    static inline Tuple_t* EXCEPTION = reinterpret_cast<Tuple_t*>(0xffffffff'ffffffff);

    // Tuple_t* tuple;
    std::atomic<Tuple_t*> tuple{nullptr};
    // char __cache_align[64-16];

    TupleFuture() : AbstractFuture{}, tuple(nullptr) {}
    TupleFuture(Tuple_t* tuple) : AbstractFuture{}, tuple(tuple) {}

    // not threadsafe
    Tuple_t* get() {
        if (tuple == EXCEPTION) [[unlikely]] { // fast path
            return nullptr;
        } else if (tuple) [[likely]] {
            return tuple;
        }

        return wait();
    }

private:
    Tuple_t* wait() {
        while (true) {
            if (auto pkt = this->pkt.load(std::memory_order_relaxed)) [[likely]] {
                auto res = pkt->as<msg::TupleGetRes>();
                if (res->mode == AccessMode::INVALID) [[unlikely]] {
                    tuple = EXCEPTION;
                    pkt->free();
                    return nullptr;
                }
                tuple = reinterpret_cast<Tuple_t*>(res->tuple);
                return tuple;
            }

            if (tuple == EXCEPTION) [[unlikely]] {
                return nullptr;
            } else if (tuple) [[likely]] {
                return tuple;
            }

            __builtin_ia32_pause();
        }
    }
};


template <typename Parse_fn>
struct SwitchFuture final : public AbstractFuture {
    Parse_fn parse_fn;

    SwitchFuture(Parse_fn&& parse_fn)
        : AbstractFuture{}, parse_fn(parse_fn) {}

    const auto get() { // can be only called once
        auto pkt = get_pkt();
        auto ret = parse_fn(pkt);
        pkt->free();
        return ret;
    }
};
