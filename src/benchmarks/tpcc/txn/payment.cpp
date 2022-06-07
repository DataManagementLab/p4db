#include "../transaction.hpp"


namespace benchmark {
namespace tpcc {

TPCC::RC TPCC::operator()(TPCCArgs::Payment& arg) {
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_txns);

    if (arg.on_switch) {
        auto now = datetime_t::now();

        auto _warehouse_f = read(warehouse, Warehouse::pk(arg.w_id));
        check(_warehouse_f);
        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_warehouse_read);
        auto _district_f = read(district, District::pk(arg.w_id, arg.d_id));
        check(_district_f);
        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_district_read);
        auto _customer_f = write(customer, Customer::pk(arg.c_w_id, arg.c_d_id, arg.c_id)); // Customer select by name not implemented
        check(_customer_f);
        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_customer_write);

        const auto _warehouse = _warehouse_f->get();
        check(_warehouse);
        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_warehouse_f_get);
        const auto _district = _district_f->get();
        check(_district);
        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_district_f_get);
        auto _customer = _customer_f->get();
        check(_customer);
        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_customer_f_get);

        // ** Optionally as Switch Transaction - Begin **
        // _warehouse->w_ytd += arg.h_amount;
        // _district->d_ytd += arg.h_amount;
        WorkerContext::get().cycl.start(stats::Cycles::switch_txn_latency);
        auto payment_f = atomic(p4_switch, TPCCSwitchInfo::Payment{Warehouse::pk(arg.w_id), District::pk(arg.w_id, arg.d_id), arg.h_amount});
        payment_f->get();
        WorkerContext::get().cycl.stop(stats::Cycles::switch_txn_latency);
        // ** Optionally as Switch Transaction - End **

        _customer->c_balance -= arg.h_amount;
        _customer->c_payment_cnt += 1;

        if (std::memcmp(_customer->c_credit, "BC", 2) == 0) {
            char c_new_data[sizeof(_customer->c_data)];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
            snprintf(c_new_data, sizeof(c_new_data), "| %4d %2d %4d %2d %4d $%5d %lu %s",
                     (int)_customer->c_id, (int)_customer->c_d_id, (int)_customer->c_w_id,
                     (int)_district->d_id, (int)_warehouse->w_id, (int)arg.h_amount, now.value, _customer->c_data);
#pragma GCC diagnostic pop

            static_assert(sizeof(_customer->c_data) == sizeof(c_new_data));
            std::memcpy(_customer->c_data, c_new_data, sizeof(c_new_data));
        }

        auto _history_f = insert(history);
        check(_history_f);
        auto _history = _history_f->get();
        check(_history);
        _history->h_c_id = arg.c_id;
        _history->h_c_d_id = _customer->c_d_id;
        _history->h_c_w_id = _customer->c_w_id;
        _history->h_d_id = arg.d_id;
        _history->h_w_id = arg.w_id;
        _history->h_amount = arg.h_amount;
        _history->h_date = now;

        snprintf(_history->h_data, sizeof(_history->h_data), "%10s    %10s", _warehouse->w_name, _district->d_name);
        static_assert(sizeof(_history->h_data) - 1 == (sizeof(_warehouse->w_name) - 1 + sizeof(_district->d_name) - 1 + 4));

        do_not_optimize(_warehouse);
        do_not_optimize(_district);
        do_not_optimize(_customer);

        WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_commits);

        return commit();
    }
    auto now = datetime_t::now();

    auto _warehouse_f = write(warehouse, Warehouse::pk(arg.w_id));
    check(_warehouse_f);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_warehouse_write);
    auto _district_f = write(district, District::pk(arg.w_id, arg.d_id));
    check(_district_f);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_district_write);
    auto _customer_f = write(customer, Customer::pk(arg.c_w_id, arg.c_d_id, arg.c_id));
    check(_customer_f);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_customer_write);

    auto _warehouse = _warehouse_f->get();
    check(_warehouse);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_warehouse_f_get);
    auto _district = _district_f->get();
    check(_district);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_district_f_get);
    auto _customer = _customer_f->get();
    check(_customer);
    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_customer_f_get);

    // ** Optionally as Switch Transaction - Begin **
    _warehouse->w_ytd += arg.h_amount;
    _district->d_ytd += arg.h_amount;
    // ** Optionally as Switch Transaction - End **

    _customer->c_balance -= arg.h_amount;
    _customer->c_payment_cnt += 1;

    if (std::memcmp(_customer->c_credit, "BC", 2) == 0) {
        char c_new_data[sizeof(_customer->c_data)];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
        snprintf(c_new_data, sizeof(c_new_data), "| %4d %2d %4d %2d %4d $%5d %lu %s",
                 (int)_customer->c_id, (int)_customer->c_d_id, (int)_customer->c_w_id,
                 (int)_district->d_id, (int)_warehouse->w_id, (int)arg.h_amount, now.value, _customer->c_data);
#pragma GCC diagnostic pop

        static_assert(sizeof(_customer->c_data) == sizeof(c_new_data));
        std::memcpy(_customer->c_data, c_new_data, sizeof(c_new_data));
    }

    auto _history_f = insert(history);
    check(_history_f);
    auto _history = _history_f->get();
    check(_history);
    _history->h_c_id = arg.c_id;
    _history->h_c_d_id = _customer->c_d_id;
    _history->h_c_w_id = _customer->c_w_id;
    _history->h_d_id = arg.d_id;
    _history->h_w_id = arg.w_id;
    _history->h_amount = arg.h_amount;
    _history->h_date = now;

    snprintf(_history->h_data, sizeof(_history->h_data), "%10s    %10s", _warehouse->w_name, _district->d_name);
    static_assert(sizeof(_history->h_data) - 1 == (sizeof(_warehouse->w_name) - 1 + sizeof(_district->d_name) - 1 + 4));

    do_not_optimize(_warehouse);
    do_not_optimize(_district);
    do_not_optimize(_customer);

    WorkerContext::get().cntr.incr(stats::Counter::tpcc_pay_commits);

    return commit();
}

} // namespace tpcc
} // namespace benchmark