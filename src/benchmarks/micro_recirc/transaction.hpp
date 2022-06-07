#pragma once

#include "args.hpp"
#include "db/transaction.hpp"
#include "table.hpp"

#include <array>
#include <cstdint>
#include <variant>


namespace benchmark {
namespace micro_recirc {


struct MicroRecirc final : public TransactionBase<MicroRecirc, MicroRecircArgs, MicroRecircTableInfo> {
    MicroRecirc(Database& db) : TransactionBase(db) {
        link_tables(db);
    }

    RC operator()(MicroRecircArgs::Arg& arg);
};


} // namespace micro_recirc
} // namespace benchmark