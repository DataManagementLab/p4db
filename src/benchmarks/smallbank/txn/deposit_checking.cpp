#include "../transaction.hpp"


namespace benchmark {
namespace smallbank {


Smallbank::RC Smallbank::operator()(SmallbankArgs::DepositChecking& arg) {
    if (arg.val < 0) {
        return rollback();
    }

    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, SmallbankSwitchInfo::DepositChecking{arg.customer_id, arg.val});
        txn_f->get();
        WorkerContext::get().cntr.incr(stats::Counter::smallbank_deposit_checking_commits);
        return commit();
    }


    auto checking_f = write(checking, Checking::pk(arg.customer_id));
    check(checking_f);
    auto checking = checking_f->get();
    check(checking);
    checking->balance += arg.val;

    WorkerContext::get().cntr.incr(stats::Counter::smallbank_deposit_checking_commits);
    return commit();
}

} // namespace smallbank
} // namespace benchmark