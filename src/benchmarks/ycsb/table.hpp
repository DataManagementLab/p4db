#pragma once

#include "db/database.hpp"
#include "switch.hpp"
#include "table/table.hpp"


namespace benchmark {
namespace ycsb {

struct YCSBTableInfo {
    template <typename T>
    using Table_t = StructTable<T>;

    struct KV {
        static constexpr auto TABLE_NAME = "kvs";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t id;
        uint32_t value;

        static constexpr p4db::key_t pk(uint64_t id) {
            return p4db::key_t{id};
        }

        void print() {
            if (value != 0) {
                std::cout << "id=" << id << " value=" << value << '\n';
            }
        }
    };

    Table_t<KV>* kvs;
    YCSBSwitchInfo p4_switch;

    using tables = parameter_pack<Table_t<KV>>;

    void link_tables(Database& db) {
        db.get_casted(KV::TABLE_NAME, kvs);
    }
};
} // namespace ycsb
} // namespace benchmark