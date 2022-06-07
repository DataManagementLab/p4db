#pragma once

#include "db/database.hpp"
#include "db/transaction.hpp"

#include <atomic>
#include <cstdint>


namespace benchmark {
namespace smallbank {


int smallbank();
void smallbank_worker(int id, Database& db, TxnExecutorStats& stats);


} // namespace smallbank
} // namespace benchmark