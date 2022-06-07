#include "../transaction.hpp"


namespace benchmark {
namespace smallbank {

Smallbank::RC Smallbank::operator()(SmallbankArgs::TransactSaving& arg) {
    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, SmallbankSwitchInfo::TransactSaving{arg.customer_id, arg.val});
        txn_f->get();
        WorkerContext::get().cntr.incr(stats::Counter::smallbank_transact_saving_commits);
        return commit();
    }

    auto saving_f = write(saving, Saving::pk(arg.customer_id)); // Opportunity for lock upgrade
    check(saving_f);
    auto saving = saving_f->get();
    check(saving);

    if ((saving->balance + arg.val) < 0) {
        return rollback();
    }
    saving->balance += arg.val;

    WorkerContext::get().cntr.incr(stats::Counter::smallbank_transact_saving_commits);
    return commit();
}

} // namespace smallbank
} // namespace benchmark