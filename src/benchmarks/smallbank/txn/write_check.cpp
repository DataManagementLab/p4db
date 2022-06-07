#include "../transaction.hpp"


namespace benchmark {
namespace smallbank {


Smallbank::RC Smallbank::operator()(SmallbankArgs::WriteCheck& arg) {
    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, SmallbankSwitchInfo::WriteCheck{arg.customer_id, arg.val});
        txn_f->get();
        WorkerContext::get().cntr.incr(stats::Counter::smallbank_write_check_commits);
        return commit();
    }


    auto saving_f = read(saving, Saving::pk(arg.customer_id));
    check(saving_f);
    auto checking_f = write(checking, Checking::pk(arg.customer_id));
    check(checking_f);

    const auto saving = saving_f->get();
    check(saving);
    auto checking = checking_f->get();
    check(checking);

    if ((saving->balance + checking->balance) < arg.val) {
        checking->balance -= (arg.val + 1);
    } else {
        checking->balance -= arg.val;
    }

    WorkerContext::get().cntr.incr(stats::Counter::smallbank_write_check_commits);
    return commit();
}

} // namespace smallbank
} // namespace benchmark