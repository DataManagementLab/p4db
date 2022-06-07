#include "../transaction.hpp"


namespace benchmark {
namespace smallbank {

Smallbank::RC Smallbank::operator()(SmallbankArgs::SendPayment& arg) {
    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, SmallbankSwitchInfo::SendPayment{arg.customer_id_1, arg.customer_id_2, arg.val});
        auto& _switch_payment = txn_f->get();
        if (_switch_payment.abort) {
            return rollback();
        }
        WorkerContext::get().cntr.incr(stats::Counter::smallbank_send_payment_commits);
        return commit();
    }

    auto checking_1_f = write(checking, Checking::pk(arg.customer_id_1)); // Opportunity for lock upgrade
    check(checking_1_f);
    auto checking_1 = checking_1_f->get();
    check(checking_1);

    if (checking_1->balance < arg.val) {
        return rollback();
    }

    auto checking_2_f = write(checking, Checking::pk(arg.customer_id_2));
    check(checking_2_f);
    auto checking_2 = checking_2_f->get();
    check(checking_2);

    checking_1->balance -= arg.val;
    checking_2->balance += arg.val;

    WorkerContext::get().cntr.incr(stats::Counter::smallbank_send_payment_commits);
    return commit();
}

} // namespace smallbank
} // namespace benchmark