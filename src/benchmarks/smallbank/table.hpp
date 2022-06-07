#pragma once

#include "switch.hpp"
#include "table/table.hpp"


namespace benchmark {
namespace smallbank {

struct SmallbankTableInfo {
    template <typename T>
    using Table_t = StructTable<T>;

    struct Customer {
        static constexpr auto TABLE_NAME = "customer";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t id;
        char name[16];

        static constexpr auto pk(uint64_t id) {
            return p4db::key_t{id};
        }

        void print() {}
    };

    struct Saving {
        static constexpr auto TABLE_NAME = "saving";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t id;
        int32_t balance;

        static constexpr auto pk(uint64_t id) {
            return p4db::key_t{id};
        }

        void print() {
            if (balance != 0) {
                std::cout << "id=" << id << " bal=" << balance << '\n';
            }
        }
    };

    struct Checking {
        static constexpr auto TABLE_NAME = "checking";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t id;
        int32_t balance;

        static constexpr auto pk(uint64_t id) {
            return p4db::key_t{id};
        }

        void print() {
            if (balance != 0) {
                std::cout << "id=" << id << " bal=" << balance << '\n';
            }
        }
    };

    Table_t<Customer>* customer;
    Table_t<Saving>* saving;
    Table_t<Checking>* checking;

    SmallbankSwitchInfo p4_switch;

    using tables = parameter_pack<Table_t<Customer>, Table_t<Saving>, Table_t<Checking>>;

    void link_tables(Database& db) {
        db.get_casted(Customer::TABLE_NAME, customer);
        db.get_casted(Saving::TABLE_NAME, saving);
        db.get_casted(Checking::TABLE_NAME, checking);
    }
};


} // namespace smallbank
} // namespace benchmark