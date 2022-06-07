#pragma once

#include "db/mempools.hpp"


// without virtual, is somehow slower
template <typename Table_t>
struct WriteVal {
    Table_t* table;
    p4db::key_t index;

    WriteVal(Table_t* table, p4db::key_t index)
        : table(table), index(index) {}

    void clear() {
        table->template unlock<AccessMode::WRITE>(index);
    }
};
template <typename Table_t>
struct ReadVal {
    Table_t* table;
    p4db::key_t index;

    ReadVal(Table_t* table, p4db::key_t index) : table(table), index(index) {}

    void clear() {
        table->template unlock<AccessMode::READ>(index);
    }
};


template <typename Tables_t>
struct Undolog {
    using Action = typename Tables_t::wrap2<WriteVal, ReadVal>::apply<std::variant>;
    std::vector<Action> actions;


    template <typename Table_t>
    void add_write(Table_t* table, p4db::key_t index) {
        actions.emplace_back(WriteVal<Table_t>{table, index});
    }

    template <typename Table_t>
    void add_read(Table_t* table, p4db::key_t index) {
        actions.emplace_back(ReadVal<Table_t>{table, index});
    }

    void commit() {
        clear();
    }

    void rollback() {
        clear();
    }

private:
    void clear() {
        for (auto& action : actions) {
            std::visit([](auto& action) {
                action.clear();
            },
                       action);
        }
        actions.clear();
    }
};