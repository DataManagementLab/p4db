#pragma once

#include "args.hpp"
#include "db/transaction.hpp"
#include "table.hpp"

#include <array>
#include <cstdint>
#include <variant>


namespace benchmark {
namespace tpcc {

struct TPCC final : public TransactionBase<TPCC, TPCCArgs, TPCCTableInfo> {
    TPCC(Database& db) : TransactionBase(db) {
        link_tables(db);
    }

    RC operator()(TPCCArgs::NewOrder& arg);
    RC operator()(TPCCArgs::Payment& arg);
};


} // namespace tpcc
} // namespace benchmark