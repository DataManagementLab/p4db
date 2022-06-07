#include "tpcc.hpp"

#include "db/config.hpp"
#include "random.hpp"
#include "transaction.hpp"
#include "utils.hpp"

#include <algorithm>
#include <thread>
#include <vector>


namespace benchmark {
namespace tpcc {


void tpcc_worker(int id, Database& db, TxnExecutorStats& stats) {
    auto& config = Config::instance();

    TPCCHotInfo hot_info{config.tpcc.num_warehouses, config.switch_entries};


    TPCCRandom rnd(config.node_id << 16 | id);
    auto wh_per_node = config.tpcc.num_warehouses / config.num_nodes;
    uint64_t home_w_id = config.tpcc.home_w_id + (wh_per_node * id) / config.num_txn_workers;

    if (!(config.tpcc.home_w_id <= home_w_id && home_w_id < (config.tpcc.home_w_id + config.tpcc.num_warehouses / config.num_nodes))) {
        throw std::runtime_error("is_home_wh failed");
    }

    std::vector<TPCCArgs::Arg_t> txns;
    txns.reserve(config.num_txns);
    for (size_t i = 0; i < config.num_txns; ++i) {
        // tpc-c_v5.11.0.pdf -> pp. 28
        auto new_order = [&]() -> TPCCArgs::Arg_t {
            uint64_t w_id = home_w_id; // constant over the whole measurement interval
            uint64_t d_id = rnd.random<uint64_t>(1, DISTRICTS_PER_WAREHOUSE) - 1;
            uint64_t c_id = rnd.NURand<uint64_t>(1023, 1, CUSTOMER_PER_DISTRICT) - 1;

            // neword(t_num, w_id, d_id, c_id, ol_cnt, all_local, itemid, supware, qty);
            TPCCArgs::NewOrder txn{w_id, d_id, c_id};
            txn.ol_cnt = rnd.random<uint64_t>(ORDER_CNT_MIN, ORDER_CNT_MAX);

            for (size_t i = 0; i < txn.ol_cnt; ++i) {
                TPCCArgs::NewOrder::OrderItem item;
                item.ol_i_id = rnd.NURand<uint64_t>(8191, 1, NUM_ITEMS) - 1;
                item.ol_supply_w_id = w_id;
                item.ol_quantity = rnd.random<uint64_t>(1, 10);

                // cause rollback 1%
                if ((i == txn.ol_cnt - 1) && rnd.random<int>(1, 100) == 1) {
                    item.ol_i_id = NUM_ITEMS + 1;
                }

                // access remote warehouse 1%
                if (rnd.random<int>(1, 100) <= config.tpcc.new_order_remote_prob && config.tpcc.num_warehouses > 1) {
                    do {
                        item.ol_supply_w_id = rnd.random<uint64_t>(1, config.tpcc.num_warehouses) - 1;
                    } while (item.ol_supply_w_id == w_id);
                    // std::cout << "remote item key=" << TPCCTableInfo::Stock::pk(item.ol_supply_w_id, item.ol_i_id)
                    //     << " item.ol_supply_w_id=" << item.ol_supply_w_id << " w_id=" << w_id << "\n";
                }

                item.is_hot = hot_info.is_hot(item.ol_supply_w_id, item.ol_i_id);

                // remove duplicates
                bool found = false;
                for (uint64_t j = 0; j < i; ++j) {
                    auto& a = txn.orders[j];
                    auto& b = item;
                    if ((a.ol_i_id == b.ol_i_id) && (a.ol_supply_w_id == b.ol_supply_w_id)) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    --i;
                    continue;
                }

                txn.orders[i] = item;
            }

            std::sort(txn.orders.begin(), txn.orders.begin() + txn.ol_cnt, [](const auto& a, const auto& b) {
                return a.ol_i_id < b.ol_i_id;
            });
            txn.on_switch = config.use_switch;
            return txn;
        };

        // tpc-c_v5.11.0.pdf -> pp. 33
        auto payment = [&]() -> TPCCArgs::Arg_t {
            uint64_t w_id = home_w_id; // constant over the whole measurement interval
            uint64_t d_id = rnd.random<uint64_t>(1, DISTRICTS_PER_WAREHOUSE) - 1;

            uint64_t c_w_id = w_id; // local payment
            uint64_t c_d_id = d_id;
            if (rnd.random<int>(1, 100) <= config.tpcc.payment_remote_prob) { // remote payment
                if (config.tpcc.num_warehouses > 1) {
                    do {
                        c_w_id = rnd.random<uint64_t>(1, config.tpcc.num_warehouses) - 1;
                    } while (c_w_id == w_id);
                } else {
                    c_w_id = w_id; // we only have 1 warehouse
                }
                c_d_id = rnd.random<uint64_t>(1, DISTRICTS_PER_WAREHOUSE) - 1;
            }

            uint64_t c_id = rnd.NURand<uint64_t>(1023, 1, CUSTOMER_PER_DISTRICT) - 1;
            int64_t h_amount = rnd.random<int64_t>(100, 500000);

            // payment(t_num, w_id, d_id, byname, c_w_id, c_d_id, c_id, c_last, h_amount);
            TPCCArgs::Payment txn{w_id, d_id, /*by_name*/ c_w_id, c_d_id, c_id, /*c_last*/ h_amount};
            txn.on_switch = config.use_switch; // Everything fits on the switch! :)
            return txn;
        };

        auto txn = (rnd.random<int>(1, 100) <= FREQUENCY_NEW_ORDER) ? new_order() : payment();
        txns.push_back(txn);
    }

    db.msg_handler->barrier.wait_workers();

    stats = txn_executor<TPCC>(db, txns);
    stats.count_on_switch(txns);
}


int tpcc() {
    auto& config = Config::instance();

    Database db;

    TPCCRandom rnd{config.node_id};

    // multiple warehouses can be local on the same node
    auto is_home_wh = [&](uint64_t w_id) {
        return config.tpcc.home_w_id <= w_id && w_id < (config.tpcc.home_w_id + config.tpcc.num_warehouses / config.num_nodes);
    };

    // tpc-c_v5.11.0.pdf -> pp. 65
    {
        using Warehouse = TPCCTableInfo::Warehouse;
        auto table = db.make_table<StructTable<Warehouse>>(Warehouse::TABLE_NAME, config.tpcc.num_warehouses);

        for (uint64_t w_id = 0; w_id < config.tpcc.num_warehouses; ++w_id) {
            p4db::key_t index;
            auto& tuple = table->insert(index);
            tuple.w_id = w_id;
            rnd.astring(6, 10, tuple.w_name);
            rnd.astring(10, 20, tuple.w_street_1);
            rnd.astring(10, 20, tuple.w_street_2);
            rnd.astring(10, 20, tuple.w_city);
            rnd.astring(2, 2, tuple.w_state);
            rnd.nstring(9, 9, tuple.w_zip);
            tuple.w_tax = rnd.random<decltype(tuple.w_tax)>(0, 2000);
            tuple.w_ytd = 30000000;

            if (index != tuple.pk()) {
                throw std::runtime_error("warehouse.pk() bad");
            }
            auto loc_info = table->part_info.location(index);
            if (loc_info.is_local != is_home_wh(tuple.w_id)) {
                std::cout << "tuple.w_id=" << tuple.w_id << " home_w_id=" << config.tpcc.home_w_id << " is_local=" << loc_info.is_local << '\n';
                throw std::runtime_error("warehouse.is_local bad");
            }
        }
        table->size = config.tpcc.num_warehouses;
    }
    {
        using District = TPCCTableInfo::District;
        auto table = db.make_table<StructTable<District>>(District::TABLE_NAME, config.tpcc.num_districts);

        for (uint64_t w_id = 0; w_id < config.tpcc.num_warehouses; ++w_id) {
            for (uint64_t d_id = 0; d_id < DISTRICTS_PER_WAREHOUSE; ++d_id) {
                p4db::key_t index;
                auto& tuple = table->insert(index);
                tuple.d_id = d_id;
                tuple.d_w_id = w_id;
                rnd.astring(6, 10, tuple.d_name);
                rnd.astring(10, 20, tuple.d_street_1);
                rnd.astring(10, 20, tuple.d_street_2);
                rnd.astring(10, 20, tuple.d_city);
                rnd.astring(2, 2, tuple.d_state);
                rnd.nstring(9, 9, tuple.d_zip);
                tuple.d_tax = rnd.random<decltype(tuple.d_tax)>(0, 2000);
                tuple.d_ytd = 3000000;
                tuple.d_next_o_id = 3001;

                if (index != tuple.pk()) {
                    throw std::runtime_error("district.pk() bad");
                }
                auto loc_info = table->part_info.location(index);
                if (loc_info.is_local != is_home_wh(tuple.d_w_id)) {
                    std::cout << "tuple.d_w_id=" << tuple.d_w_id << " home_w_id=" << config.tpcc.home_w_id << " is_local=" << loc_info.is_local << '\n';
                    throw std::runtime_error("district.is_local bad");
                }
            }
        }
    }
    {
        using Customer = TPCCTableInfo::Customer;
        auto table = db.make_table<StructTable<Customer>>(Customer::TABLE_NAME, config.tpcc.num_districts * CUSTOMER_PER_DISTRICT);

        for (uint64_t w_id = 0; w_id < config.tpcc.num_warehouses; ++w_id) {
            for (uint64_t d_id = 0; d_id < DISTRICTS_PER_WAREHOUSE; ++d_id) { // foreach in district
                for (uint64_t c_id = 0; c_id < CUSTOMER_PER_DISTRICT; ++c_id) {
                    p4db::key_t index;
                    auto& tuple = table->insert(index);
                    tuple.c_id = c_id;
                    tuple.c_d_id = d_id;
                    tuple.c_w_id = w_id;

                    if (c_id < 1000) {
                        rnd.cLastName(c_id, tuple.c_last);
                    } else {
                        rnd.cLastName(rnd.NURand<int>(255, 0, 999), tuple.c_last);
                    }

                    tuple.c_middle[0] = 'O';
                    tuple.c_middle[1] = 'E';
                    tuple.c_middle[2] = '\0';

                    rnd.astring(8, 16, tuple.c_first);
                    rnd.astring(10, 20, tuple.c_street_1);
                    rnd.astring(10, 20, tuple.c_street_2);
                    rnd.astring(10, 20, tuple.c_city);
                    rnd.astring(2, 2, tuple.c_state);
                    rnd.nstring(9, 9, tuple.c_zip);
                    rnd.nstring(16, 16, tuple.c_phone);
                    tuple.c_since = datetime_t::now();

                    if (rnd.random<int>(1, 100) <= 10) {
                        tuple.c_credit[0] = 'G';
                    } else {
                        tuple.c_credit[0] = 'B';
                    }
                    tuple.c_credit[1] = 'C';
                    tuple.c_credit[2] = '\0';

                    tuple.c_credit_lim = 5000000;
                    tuple.c_discount = rnd.random<decltype(tuple.c_discount)>(0, 50000);
                    tuple.c_balance = -1000;
                    tuple.c_ytd_payment = 1000;
                    tuple.c_payment_cnt = 1;
                    tuple.c_delivery_cnt = 0;
                    rnd.astring(300, 500, tuple.c_data);

                    if (index != tuple.pk()) {
                        throw std::runtime_error("customer.pk() bad");
                    }
                    auto loc_info = table->part_info.location(index);
                    if (loc_info.is_local != is_home_wh(tuple.c_w_id)) {
                        std::cout << "tuple.c_w_id=" << tuple.c_w_id << " home_w_id=" << config.tpcc.home_w_id << " is_local=" << loc_info.is_local << '\n';
                        throw std::runtime_error("customer.is_local bad");
                    }
                }
            }
        }
    }
    {
        using Item = TPCCTableInfo::Item;
        auto table = db.make_table<StructTable<Item>>(Item::TABLE_NAME, NUM_ITEMS);

        for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
            p4db::key_t index;
            auto& tuple = table->insert(index);
            ;
            tuple.i_id = i;
            tuple.i_im_id = rnd.random<decltype(tuple.i_im_id)>(1, 10000);
            rnd.astring(14, 24, tuple.i_name);
            tuple.i_price = rnd.random<decltype(tuple.i_price)>(100, 10000);

            rnd.astring(26, 50, tuple.i_data);
            int i_data_len = strlen(tuple.i_data);
            if (rnd.random<int>(1, 100) <= 10) {
                int pos = rnd.random<int>(0, i_data_len - 8);
                tuple.i_data[pos] = 'O';
                tuple.i_data[pos + 1] = 'R';
                tuple.i_data[pos + 2] = 'I';
                tuple.i_data[pos + 3] = 'G';
                tuple.i_data[pos + 4] = 'I';
                tuple.i_data[pos + 5] = 'N';
                tuple.i_data[pos + 6] = 'A';
                tuple.i_data[pos + 7] = 'L';
            }
            auto loc_info = table->part_info.location(index);
            if (!loc_info.is_local) {
                std::cout << "item is_local=" << loc_info.is_local << '\n';
                throw std::runtime_error("item.is_local bad");
            }
        }
    }
    {
        using Stock = TPCCTableInfo::Stock;
        auto table = db.make_table<StructTable<Stock>>(Stock::TABLE_NAME, config.tpcc.num_warehouses * NUM_ITEMS);

        for (uint64_t w_id = 0; w_id < config.tpcc.num_warehouses; ++w_id) {
            for (uint64_t s_i_id = 0; s_i_id < NUM_ITEMS; ++s_i_id) {
                p4db::key_t index;
                auto& tuple = table->insert(index);
                tuple.s_i_id = s_i_id;
                tuple.s_w_id = w_id;
                tuple.s_quantity = rnd.random<int>(10, 100);
                for (auto& s_dist : tuple.s_dist) {
                    rnd.astring(24, 24, s_dist);
                }
                tuple.s_ytd = 0;
                tuple.s_order_cnt = 0;
                tuple.s_remote_cnt = 0;

                rnd.astring(26, 50, tuple.s_data);
                int s_data_len = strlen(tuple.s_data);
                if (rnd.random<int>(1, 100) <= 10) {
                    int pos = rnd.random<int>(0L, s_data_len - 8);
                    tuple.s_data[pos] = 'O';
                    tuple.s_data[pos + 1] = 'R';
                    tuple.s_data[pos + 2] = 'I';
                    tuple.s_data[pos + 3] = 'G';
                    tuple.s_data[pos + 4] = 'I';
                    tuple.s_data[pos + 5] = 'N';
                    tuple.s_data[pos + 6] = 'A';
                    tuple.s_data[pos + 7] = 'L';
                }

                if (index != tuple.pk(w_id, s_i_id)) {
                    throw std::runtime_error("stock.pk(w_id, s_i_id) bad");
                }
                auto loc_info = table->part_info.location(index);
                if (loc_info.is_local != is_home_wh(tuple.s_w_id)) {
                    std::cout << "tuple.s_w_id=" << tuple.s_w_id << " home_w_id=" << config.tpcc.home_w_id << " is_local=" << loc_info.is_local << '\n';
                    throw std::runtime_error("stock.is_local bad");
                }
            }
        }
    }
    {
        using Order = TPCCTableInfo::Order;
        db.make_table<StructTable<Order>>(Order::TABLE_NAME, config.num_txn_workers * config.num_txns);
        // only to fill up for now
    }
    {
        using NewOrder = TPCCTableInfo::NewOrder;
        db.make_table<StructTable<NewOrder>>(NewOrder::TABLE_NAME, config.num_txn_workers * config.num_txns);
        // only to fill up for now
    }
    {
        using OrderLine = TPCCTableInfo::OrderLine;
        db.make_table<StructTable<OrderLine>>(OrderLine::TABLE_NAME, config.num_txn_workers * config.num_txns * TPCCArgs::NewOrder::MAX_ORDERS);
        // only to fill up for now
    }
    {
        using History = TPCCTableInfo::History;
        db.make_table<StructTable<History>>(History::TABLE_NAME, config.num_txn_workers * config.num_txns);
        // only to fill up for now
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
            tpcc_worker(i, db, stat);
        }));
    }

    for (auto& w : workers) {
        w.join();
    }
    std::cout << TxnExecutorStats::accumulate(stats) << '\n';
    db.msg_handler->barrier.wait_nodes();

    if (!config.verify) {
        return 0;
    }

    std::cerr << "Starting consistency checks...\n";
    TPCCTableInfo ti;
    ti.link_tables(db);

    // 3.3.2.1 Consistency Condition 1
    for (auto& w : *ti.warehouse) {
        int64_t w_ytd = w.w_ytd;
        int64_t total = 0;
        for (auto& d : *ti.district) {
            if (d.d_w_id != w.w_id) {
                continue;
            }
            total += d.d_ytd;
        }
        // std::cout << "w_id=" << w.w_id << " w_ytd=" << w_ytd << " total=" << total << '\n';
        if (w_ytd != total) {
            std::cerr << "w_ytd != sum(d_ytd) -> " << w_ytd << " != " << total << '\n';
        }
    }

    // 3.3.2.2 Consistency Condition 2
    for (auto& d : *ti.district) {
        auto d_next_o_id = d.d_next_o_id;
        uint64_t max_o_id = 0;
        bool o_has_rows = false;
        for (auto& o : *ti.order) {
            if (o.o_w_id != d.d_w_id || o.o_d_id != d.d_id) {
                continue;
            }
            max_o_id = std::max(max_o_id, o.o_id);
            o_has_rows = true;
        }
        if (o_has_rows && max_o_id != d_next_o_id - 1) {
            std::cerr << "d_next_o_id-1 != max_o_id -> " << (d_next_o_id - 1) << " != " << max_o_id << '\n';
        }

        uint64_t max_no_o_id = 0;
        bool no_has_rows = false;
        for (auto& no : *ti.new_order) {
            if (no.no_w_id != d.d_w_id || no.no_d_id != d.d_id) {
                continue;
            }
            max_no_o_id = std::max(max_no_o_id, no.no_o_id);
            no_has_rows = true;
        }
        if (no_has_rows && max_no_o_id != d_next_o_id - 1) {
            std::cerr << "d_next_o_id-1 != max_no_o_id -> " << (d_next_o_id - 1) << " != " << max_no_o_id << '\n';
        }
    }

    // 3.3.2.3 Consistency Condition 3
    for (auto& d : *ti.district) {
        uint64_t max_no_o_id = 0;
        uint64_t min_no_o_id = -1;
        uint64_t num_rows = 0;
        for (auto& no : *ti.new_order) {
            if (no.no_w_id != d.d_w_id || no.no_d_id != d.d_id) {
                continue;
            }
            max_no_o_id = std::max(max_no_o_id, no.no_o_id);
            min_no_o_id = std::min(min_no_o_id, no.no_o_id);
            ++num_rows;
        }
        if (num_rows && (max_no_o_id - min_no_o_id + 1) != num_rows) {
            std::cerr << "(max_no_o_id - min_no_o_id + 1) != num_rows -> "
                      << (max_no_o_id - min_no_o_id + 1) << " != " << num_rows << '\n';
        }
    }

    // 3.3.2.4 Consistency Condition 4
    for (auto& d : *ti.district) {
        uint64_t sum_o_ol_cnt = 0;
        for (auto& o : *ti.order) {
            if (o.o_w_id != d.d_w_id || o.o_d_id != d.d_id) {
                continue;
            }
            sum_o_ol_cnt += o.o_ol_cnt;
        }
        uint64_t num_rows = 0;
        for (auto& ol : *ti.order_line) {
            if (ol.ol_w_id != d.d_w_id || ol.ol_d_id != d.d_id) {
                continue;
            }
            ++num_rows;
        }
        if (sum_o_ol_cnt != num_rows) {
            std::cerr << "sum_o_ol_cnt != num_rows -> " << sum_o_ol_cnt << " != " << num_rows << '\n';
        }
    }

    // 3.3.2.8 Consistency Condition 8
    for (auto& w : *ti.warehouse) {
        int64_t sum_h_amount = 30000000; // HINT change to 0 when history table populated
        for (auto& h : *ti.history) {
            if (h.h_w_id != w.w_id) {
                continue;
            }
            sum_h_amount += h.h_amount;
        }
        if (sum_h_amount != w.w_ytd) {
            std::cerr << "sum_h_amount != w.w_ytd -> " << sum_h_amount << " != " << w.w_ytd << '\n';
        }
    }

    // 3.3.2.9 Consistency Condition 9
    for (auto& d : *ti.district) {
        int64_t sum_h_amount = 3000000; // HINT change to 0 when history table populated
        for (auto& h : *ti.history) {
            if (h.h_w_id != d.d_w_id || h.h_d_id != d.d_id) {
                continue;
            }
            sum_h_amount += h.h_amount;
        }
        if (sum_h_amount != d.d_ytd) {
            std::cerr << "sum_h_amount != d.d_ytd -> " << sum_h_amount << " != " << d.d_ytd << '\n';
        }
    }

    std::cerr << "Done.\n";


    return 0;
}


} // namespace tpcc
} // namespace benchmark