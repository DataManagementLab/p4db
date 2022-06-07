#include "smallbank.hpp"

#include "db/config.hpp"
#include "random.hpp"
#include "transaction.hpp"

#include <thread>
#include <utility>


namespace benchmark {
namespace smallbank {


void smallbank_worker(int id, Database& db, TxnExecutorStats& stats) {
    auto& config = Config::instance();

    SmallbankRandom rnd(config.node_id << 16 | id);

    std::vector<Smallbank::Arg_t> txns;
    txns.reserve(config.num_txns);
    for (size_t i = 0; i < config.num_txns; ++i) {
        bool is_hot_txn = rnd.is_hot_txn();

        auto get_account = [&]() {
            return is_hot_txn ? rnd.hot_cid() : rnd.cold_cid();
        };

        auto get_accounts = [&]() {
            uint64_t acc_0 = get_account();
            uint64_t acc_1 = 0;
            do {
                acc_1 = get_account();
            } while (acc_0 == acc_1); // Account IDs need to be distinct
            return std::pair(acc_0, acc_1);
        };

        auto get_txn = [&]() -> Smallbank::Arg_t {
            auto txn_type = rnd.random<int>(1, 100);
            if (txn_type <= 15) {
                auto [acc_0, acc_1] = get_accounts();
                Smallbank::Amalgamate txn;
                txn.customer_id_1 = acc_0;
                txn.customer_id_2 = acc_1;
                txn.on_switch = config.use_switch && is_hot_txn;
                return txn;
            } else if (txn_type <= 30) {
                auto acc_0 = get_account();
                Smallbank::Balance txn;
                txn.customer_id = acc_0;
                txn.on_switch = config.use_switch && is_hot_txn;
                return txn;
            } else if (txn_type <= 45) {
                auto acc_0 = get_account();
                Smallbank::DepositChecking txn;
                txn.customer_id = acc_0;
                txn.val = 130; // original (float) 1.3, we use fixed point
                txn.on_switch = config.use_switch && is_hot_txn;
                return txn;
            } else if (txn_type <= 70) {
                auto [acc_0, acc_1] = get_accounts();
                Smallbank::SendPayment txn;
                txn.customer_id_1 = acc_0;
                txn.customer_id_2 = acc_1;
                txn.val = 500;
                txn.on_switch = config.use_switch && is_hot_txn;
                return txn;
            } else if (txn_type <= 85) {
                auto acc_0 = get_account();
                Smallbank::TransactSaving txn;
                txn.customer_id = acc_0;
                txn.val = 2020;
                txn.on_switch = config.use_switch && is_hot_txn;
                return txn;
            } else if (txn_type <= 100) {
                auto acc_0 = get_account();
                Smallbank::WriteCheck txn;
                txn.customer_id = acc_0;
                txn.val = 500;
                txn.on_switch = config.use_switch && is_hot_txn;
                return txn;
            }
            throw std::runtime_error("smallbank random fail");
        };

        auto txn = get_txn();
        txns.push_back(txn);
    }

    db.msg_handler->barrier.wait_workers();

    stats = txn_executor<Smallbank>(db, txns);
    stats.count_on_switch(txns);
}


int smallbank() {
    auto& config = Config::instance();
    Database db;
    SmallbankRandom rnd(config.node_id);

    {
        using Customer = SmallbankTableInfo::Customer;
        auto table = db.make_table<StructTable<Customer>>(Customer::TABLE_NAME, config.smallbank.table_size);

        // if (!table->read_dump()) {
        for (uint64_t i = 0; i < config.smallbank.table_size; i++) {
            p4db::key_t index;
            auto& tuple = table->insert(index);
            snprintf(tuple.name, sizeof(tuple.name), "%015lu", i);
            tuple.id = i;
        }
        //     table->write_dump();
        // }
    }

    {
        using Saving = SmallbankTableInfo::Saving;
        auto table = db.make_table<StructTable<Saving>>(Saving::TABLE_NAME, config.smallbank.table_size);

        // if (!table->read_dump()) {
        for (uint64_t i = 0; i < config.smallbank.table_size; i++) {
            p4db::key_t index;
            auto& tuple = table->insert(index);
            tuple.id = i;
            tuple.balance = rnd.balance<int32_t>();
        }
        //     table->write_dump();
        // }
    }

    {
        using Checking = SmallbankTableInfo::Checking;
        auto table = db.make_table<StructTable<Checking>>(Checking::TABLE_NAME, config.smallbank.table_size);

        // if (!table->read_dump()) {
        for (uint64_t i = 0; i < config.smallbank.table_size; i++) {
            p4db::key_t index;
            auto& tuple = table->insert(index);
            tuple.id = i;
            tuple.balance = rnd.balance<int32_t>();
        }
        // table->write_dump();
        // }
    }

    db.msg_handler->barrier.wait_nodes();


    std::vector<std::thread> workers;
    workers.reserve(config.num_txn_workers);
    std::vector<TxnExecutorStats> stats;
    stats.reserve(config.num_txn_workers);

    for (uint32_t i = 0; i < config.num_txn_workers; ++i) {
        auto& stat = stats.emplace_back();
        workers.emplace_back(std::thread([&, i]() {
            const WorkerContext::guard worker_ctx;
            pin_worker(i);
            smallbank_worker(i, db, stat);
        }));
    }

    for (auto& w : workers) {
        w.join();
    }
    std::cout << TxnExecutorStats::accumulate(stats) << '\n';
    db.msg_handler->barrier.wait_nodes();

    return 0;
}


} // namespace smallbank
} // namespace benchmark