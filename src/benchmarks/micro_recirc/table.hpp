#pragma once

#include "db/database.hpp"
#include "db/defs.hpp"
#include "db/types.hpp"
#include "switch.hpp"
#include "table/table.hpp"


namespace benchmark {
namespace micro_recirc {


struct MicroRecircTableInfo {
    template <typename T>
    using Table_t = StructTable<T>;

    MicroRecircSwitchInfo p4_switch;

    void link_tables(Database&) {}
};


} // namespace micro_recirc
} // namespace benchmark