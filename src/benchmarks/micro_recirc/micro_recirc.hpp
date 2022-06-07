#pragma once

#include "db/database.hpp"
#include "db/transaction.hpp"

#include <atomic>
#include <cstdint>


namespace benchmark {
namespace micro_recirc {


int micro_recirc();
void micro_recirc_worker(int id, Database& db, TxnExecutorStats& stats);


} // namespace micro_recirc
} // namespace benchmark