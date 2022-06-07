#pragma once

#include "db/database.hpp"
#include "db/transaction.hpp"

#include <atomic>
#include <cstdint>


namespace benchmark {
namespace tpcc {

int tpcc();
void tpcc_worker(int id, Database& db, TxnExecutorStats& stats);


} // namespace tpcc
} // namespace benchmark