#include "../switch.hpp"

#include "stats/stats.hpp"


namespace benchmark {
namespace smallbank {


auto lock_bits = [](lock_t& locks, std::initializer_list<SmallbankDeclusteredLayout::TupleLocation> tuple_locations) {
    locks = lock_t{0, 0};
    for (auto& tl : tuple_locations) {
        if (tl.lock_bit == 0) { // left lock needed
            locks.left |= 1;
        } else {
            locks.right |= 1;
        }
    }
};

void SmallbankSwitchInfo::make_txn(const Amalgamate& arg, BufferWriter& bw) {

    auto info = bw.write(info_t{});
    info->multipass = 1;

    auto& tl0 = declustered_layout.get_location(arg.c_id_0);
    auto& tl1 = declustered_layout.get_location(arg.c_id_1);
    lock_bits(info->locks, {tl0, tl1});

    if constexpr (SWITCH_NO_CONFLICT) {
        info->locks = lock_t{0, 0};
        info->multipass = 0;
    }

    bw.write(amalgamate_t{InstrType_t::AMALGAMATE(tl0.stage_id), tl0.reg_array_idx, 0, 0});
    bw.write(amalgamate_egress_t{InstrType_t::AMALGAMATE_EGRESS().set_stop(true), InstrType_t::DEPOSIT_CHECKING(tl1.stage_id), tl1.reg_array_idx});
    bw.write(InstrType_t::STOP());
}

void SmallbankSwitchInfo::make_txn(const Balance& arg, BufferWriter& bw) {
    bw.write(info_t{});

    auto& tl = declustered_layout.get_location(arg.c_id);
    bw.write(balance_t{InstrType_t::BALANCE(tl.stage_id), tl.reg_array_idx, 0, 0});
    bw.write(InstrType_t::STOP());
}

void SmallbankSwitchInfo::make_txn(const DepositChecking& arg, BufferWriter& bw) {
    bw.write(info_t{});

    auto& tl = declustered_layout.get_location(arg.c_id);
    bw.write(deposit_checking_t{InstrType_t::DEPOSIT_CHECKING(tl.stage_id), tl.reg_array_idx, arg.amount});
    bw.write(InstrType_t::STOP());
}

void SmallbankSwitchInfo::make_txn(const SendPayment& arg, BufferWriter& bw) {
    auto info = bw.write(info_t{});
    info->multipass = 1;

    auto& tl0 = declustered_layout.get_location(arg.c_id_0);
    auto& tl1 = declustered_layout.get_location(arg.c_id_1);
    lock_bits(info->locks, {tl0, tl1});

    if constexpr (SWITCH_NO_CONFLICT) {
        info->locks = lock_t{0, 0};
        info->multipass = 0;
    }

    auto c_type_0 = InstrType_t::DEPOSIT_CHECKING(tl0.stage_id);
    auto c_type_1 = InstrType_t::DEPOSIT_CHECKING(tl1.stage_id);
    bw.write(deposit_checking_t{c_type_0, tl0.reg_array_idx, 0}); // deposit 0 returns current value
    auto sp_egress = InstrType_t::SEND_PAYMENT_EGRESS().set_stop(true);
    bw.write(send_payment_egress_t{sp_egress, c_type_0, c_type_1, tl1.reg_array_idx, arg.amount}); // aborts if (x1 < V)
    bw.write(InstrType_t::STOP());
}

void SmallbankSwitchInfo::make_txn(const TransactSaving& arg, BufferWriter& bw) {
    bw.write(info_t{});

    auto& tl = declustered_layout.get_location(arg.c_id);
    bw.write(transact_saving_t{InstrType_t::TRANSACT_SAVING(tl.stage_id), tl.reg_array_idx, arg.amount});
    bw.write(InstrType_t::STOP());
}

void SmallbankSwitchInfo::make_txn(const WriteCheck& arg, BufferWriter& bw) {
    auto info = bw.write(info_t{});
    info->multipass = 1;

    auto& tl = declustered_layout.get_location(arg.c_id);
    lock_bits(info->locks, {tl});

    if constexpr (SWITCH_NO_CONFLICT) {
        info->locks = lock_t{0, 0};
        info->multipass = 0;
    }

    bw.write(balance_t{InstrType_t::BALANCE(tl.stage_id), tl.reg_array_idx, 0, 0});
    bw.write(write_check_egress_t{InstrType_t::WRITE_CHECK_EGRESS().set_stop(true), tl.reg_array_idx, arg.amount});
    bw.write(InstrType_t::STOP());
}


SmallbankSwitchInfo::AmalgamateOut SmallbankSwitchInfo::parse_txn(const Amalgamate& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    auto deposit = br.read<deposit_checking_t>();
    return AmalgamateOut{*deposit->checking_bal};
}
SmallbankSwitchInfo::BalanceOut SmallbankSwitchInfo::parse_txn(const Balance& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    auto balance = br.read<balance_t>();
    return BalanceOut{*balance->saving_bal, *balance->checking_bal};
}
SmallbankSwitchInfo::DepositCheckingOut SmallbankSwitchInfo::parse_txn(const DepositChecking& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    auto deposit = br.read<deposit_checking_t>();
    return DepositCheckingOut{*deposit->checking_bal};
}
SmallbankSwitchInfo::SendPaymentOut SmallbankSwitchInfo::parse_txn(const SendPayment& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    if (*(br.peek<InstrType_t>()) == InstrType_t::ABORT()) {
        WorkerContext::get().cntr.incr(stats::Counter::switch_aborts);
        SendPaymentOut out;
        out.abort = true;
        return out;
    }
    auto deposit_0 = br.read<deposit_checking_t>();
    auto deposit_1 = br.read<deposit_checking_t>();

    SendPaymentOut out;
    out.checking_bal_0 = *deposit_0->checking_bal;
    out.checking_bal_1 = *deposit_1->checking_bal;
    return out;
}
SmallbankSwitchInfo::TransactSavingOut SmallbankSwitchInfo::parse_txn(const TransactSaving& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    auto transact = br.read<transact_saving_t>();
    return TransactSavingOut{*transact->saving_bal};
}
SmallbankSwitchInfo::WriteCheckOut SmallbankSwitchInfo::parse_txn(const WriteCheck& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    auto deposit = br.read<deposit_checking_t>();
    return WriteCheckOut{*deposit->checking_bal};
}

} // namespace smallbank
} // namespace benchmark