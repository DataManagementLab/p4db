#include "../transaction.hpp"


namespace benchmark {
namespace smallbank {

Smallbank::RC Smallbank::operator()(SmallbankArgs::Amalgamate& arg) {
    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, SmallbankSwitchInfo::Amalgamate{arg.customer_id_1, arg.customer_id_2});
        txn_f->get();
        WorkerContext::get().cntr.incr(stats::Counter::smallbank_amalgamate_commits);
        return commit();
    }


    auto saving_1_f = write(saving, Saving::pk(arg.customer_id_1));
    check(saving_1_f);
    auto checking_1_f = write(checking, Checking::pk(arg.customer_id_1));
    check(checking_1_f);
    auto checking_2_f = write(checking, Checking::pk(arg.customer_id_2));
    check(checking_2_f);

    auto saving_1 = saving_1_f->get();
    check(saving_1);
    auto checking_1 = checking_1_f->get();
    check(checking_1);
    auto checking_2 = checking_2_f->get();
    check(checking_2);

    checking_2->balance += (saving_1->balance + checking_1->balance);

    saving_1->balance = 0;
    checking_1->balance = 0;

    WorkerContext::get().cntr.incr(stats::Counter::smallbank_amalgamate_commits);
    return commit();
}

} // namespace smallbank
} // namespace benchmark