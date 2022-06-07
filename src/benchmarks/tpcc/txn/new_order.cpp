#include "../transaction.hpp"


namespace benchmark {
namespace tpcc {

TPCC::RC TPCC::operator()(TPCCArgs::NewOrder& arg) {
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_txns);

    auto now = datetime_t::now();

    // CHEAT/Optimization from H-Store: Validate all items to see if we will need to rollback.
    // Also determine if this is an all local order or not
    TupleFuture<Item>* items[TPCCArgs::NewOrder::MAX_ORDERS];
    bool all_local = true;
    for (size_t i = 0; i < arg.ol_cnt; ++i) {
        const auto& order = arg.orders[i];
        all_local &= (order.ol_supply_w_id == arg.w_id);
        items[i] = read(item, Item::pk(order.ol_i_id)); // may be out of bounds (1% chance) and cause rollback
        check(items[i]);
    }

    // we need to catch possible remote exceptions before we do any write ops. (write-set is not copied, just pointer to real locked row)
    for (size_t i = 0; i < arg.ol_cnt; ++i) {
        check(items[i]->get());
    }
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_item_acquired);

    // Modes: read, read_for_update, write
    auto _warehouse_f = read(warehouse, Warehouse::pk(arg.w_id));
    check(_warehouse_f);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_warehouse_read);

    auto _district_f = [&] {
        if (arg.on_switch) {
            return read(district, District::pk(arg.w_id, arg.d_id));
        } else {
            return write(district, District::pk(arg.w_id, arg.d_id));
        }
    }();
    check(_district_f);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_district_write);

    auto _customer_f = read(customer, Customer::pk(arg.w_id, arg.d_id, arg.c_id));
    check(_customer_f);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_customer_read);

    const auto _warehouse = _warehouse_f->get();
    check(_warehouse);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_warehouse_f_get);
    auto _district = _district_f->get();
    check(_district);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_district_f_get);

    const auto _customer = _customer_f->get();
    check(_customer);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_customer_f_get);


    // ** Optionally as Switch Transaction - Begin **
    TupleFuture<Stock>* stocks[TPCCArgs::NewOrder::MAX_ORDERS];
    for (size_t i = 0; i < arg.ol_cnt; ++i) {
        // get stockInfo
        const auto& order = arg.orders[i];
        stocks[i] = (arg.on_switch && order.is_hot) ? read(stock, Stock::pk(order.ol_supply_w_id, order.ol_i_id)) // now only a (local from replica) read
                                                    : write(stock, Stock::pk(order.ol_supply_w_id, order.ol_i_id));
        check(stocks[i]);
    }
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_stock_acquired);
    // ** Optionally as Switch Transaction - End **

    // no ABORT from here on:
    if (arg.on_switch) {
        WorkerContext::get().cycl.start(stats::Cycles::switch_txn_latency);
    }
    auto _switch_district_f = (arg.on_switch) ? atomic(p4_switch, TPCCSwitchInfo::NewOrder{District::pk(arg.w_id, arg.d_id), arg})
                                              : nullptr;

    uint64_t d_next_o_id;
    if (arg.on_switch) {
        auto& _switch_district = _switch_district_f->get();
        WorkerContext::get().cycl.stop(stats::Cycles::switch_txn_latency);
        d_next_o_id = _switch_district.d_next_o_id;
    } else {
        d_next_o_id = _district->d_next_o_id;
    }

    auto _order_f = insert(order);
    check(_order_f);
    auto _order = _order_f->get();
    check(_order);
    _order->o_id = d_next_o_id;
    _order->o_d_id = _district->d_id;
    _order->o_w_id = _warehouse->w_id;
    _order->o_entry_d = now;
    _order->o_carrier_id = 0;
    _order->o_ol_cnt = arg.ol_cnt;
    _order->o_all_local = all_local;

    auto _new_order_f = insert(new_order);
    check(_new_order_f);
    auto _new_order = _new_order_f->get();
    check(_new_order);
    _new_order->no_o_id = _order->o_id;
    _new_order->no_d_id = _district->d_id;
    _new_order->no_w_id = _warehouse->w_id;

    // ** Optionally as Switch Transaction - Begin **
    if (!arg.on_switch) {
        _district->d_next_o_id += 1;
    }
    // ** Optionally as Switch Transaction - End **

    int64_t total = 0;
    for (size_t i = 0; i < arg.ol_cnt; ++i) {
        auto& order = arg.orders[i];
        const Item* item = items[i]->get();
        Stock* stock = stocks[i]->get();
        check(stock);

        // ** Optionally as Switch Transaction - Begin **
        if (!(arg.on_switch && order.is_hot)) {
            stock->s_ytd += order.ol_quantity;
            if (stock->s_quantity >= order.ol_quantity + 10) {
                stock->s_quantity -= order.ol_quantity;
            } else {
                stock->s_quantity += 91 - order.ol_quantity;
            }
            stock->s_order_cnt++;

            if (order.ol_supply_w_id != arg.w_id) {
                stock->s_remote_cnt += 1;
            }
        }
        // ** Optionally as Switch Transaction - End **

        char brand_generic;
        if (!contains("ORIGINAL", item->i_data) && !contains("ORIGINAL", stock->s_data)) {
            brand_generic = 'B';
        } else {
            brand_generic = 'G';
        }
        do_not_optimize(brand_generic); // keep value to be returned to TPCC Console


        int64_t ol_amount = order.ol_quantity * item->i_price;
        total += ol_amount;

        // create OrderLine
        auto _order_line_f = insert(order_line);
        check(_order_line_f);
        auto _order_line = _order_line_f->get();
        check(_order_line);
        _order_line->ol_d_id = _district->d_id;
        _order_line->ol_w_id = _warehouse->w_id;
        _order_line->ol_number = i + 1;
        _order_line->ol_i_id = order.ol_i_id;
        _order_line->ol_supply_w_id = order.ol_supply_w_id;
        _order_line->ol_delivery_d = datetime_t{0};
        _order_line->ol_quantity = order.ol_quantity;
        _order_line->ol_amount = ol_amount;
        std::memcpy(_order_line->ol_dist_info, stock->s_dist[arg.d_id], sizeof(_order_line->ol_dist_info));
        static_assert(sizeof(stock->s_dist[0]) == sizeof(_order_line->ol_dist_info));
    }

    int64_t w_tax = _warehouse->w_tax; // in Scheme these are floats
    int64_t d_tax = _district->d_tax;
    int64_t c_discount = _customer->c_discount;

    total *= (1 - c_discount) * (1 + w_tax + d_tax);
    do_not_optimize(total); // keep value to be returned to TPCC Console

    WorkerContext::get().cntr.incr(stats::Counter::tpcc_no_commits);

    return commit();
}

} // namespace tpcc
} // namespace benchmark