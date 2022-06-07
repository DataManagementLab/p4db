#pragma once

#include "db/database.hpp"
#include "db/defs.hpp"
#include "db/transaction.hpp"
#include "db/types.hpp"
#include "table/table.hpp"

#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <variant>


namespace benchmark {
namespace ycsb {

int ycsb();
void ycsb_worker(int id, Database& db, TxnExecutorStats& stats);


} // namespace ycsb
} // namespace benchmark