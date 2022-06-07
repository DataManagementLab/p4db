#pragma once


#include "args.hpp"
#include "db/transaction.hpp"
#include "table.hpp"


namespace benchmark {
namespace smallbank {


struct Smallbank final : public TransactionBase<Smallbank, SmallbankArgs, SmallbankTableInfo> {
    Smallbank(Database& db) : TransactionBase(db) {
        link_tables(db);
    }

    RC operator()(SmallbankArgs::Balance& arg);
    RC operator()(SmallbankArgs::DepositChecking& arg);
    RC operator()(SmallbankArgs::TransactSaving& arg);
    RC operator()(SmallbankArgs::Amalgamate& arg);
    RC operator()(SmallbankArgs::WriteCheck& arg);
    RC operator()(SmallbankArgs::SendPayment& arg);
};


} // namespace smallbank
} // namespace benchmark