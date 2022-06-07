#include "../transaction.hpp"


namespace benchmark {
namespace smallbank {

Smallbank::RC Smallbank::operator()(SmallbankArgs::Balance& arg) {
    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, SmallbankSwitchInfo::Balance{arg.customer_id});
        txn_f->get();
        WorkerContext::get().cntr.incr(stats::Counter::smallbank_balance_commits);
        return commit();
    }

    auto saving_f = read(saving, Saving::pk(arg.customer_id));
    check(saving_f);
    auto checking_f = read(checking, Checking::pk(arg.customer_id));
    check(checking_f);

    const auto saving = saving_f->get();
    check(saving);
    const auto checking = checking_f->get();
    check(checking);

    const auto sum = saving->balance + checking->balance;
    do_not_optimize(sum);

    WorkerContext::get().cntr.incr(stats::Counter::smallbank_balance_commits);
    return commit();
}

} // namespace smallbank
} // namespace benchmark