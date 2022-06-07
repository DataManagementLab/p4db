#pragma once


#include "args.hpp"
#include "db/transaction.hpp"
#include "table.hpp"


namespace benchmark {
namespace ycsb {

struct YCSB : public TransactionBase<YCSB, YCSBArgs, YCSBTableInfo> {
    YCSB(Database& db) : TransactionBase(db) {
        link_tables(db);
    }

    RC operator()(YCSBArgs::Write& arg);
    RC operator()(YCSBArgs::Read& arg);
    RC operator()(YCSBArgs::Multi<NUM_OPS>& arg);
};

} // namespace ycsb
} // namespace benchmark