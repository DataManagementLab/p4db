#include "../switch.hpp"

#include "../table.hpp"

// Payment:
// - reg_0 Warehouse -> w_ytd (+= h_amount)
// - reg_1 District -> d_ytd (+= h_amount)
// NewOrder:
// - reg_2 District -> d_next_o_id (+= 1)

// - Stock: -> over-partition HOT items
//     - reg_3 -> s_ytd (+= order.ol_quantity)   # for each arg.ol_cnt
//     - reg_4 -> s_quantity if (s_quantity >= order.ol_quantity + 10) { s_quantity -= order.ol_quantity;} else { s_quantity += 91 - order.ol_quantity; }
//     - reg_5 -> s_order_cnt += 1
//     - reg_6 -> s_remote_cnt += 1/0  # if (order.ol_supply_w_id != arg.w_id)


namespace benchmark {
namespace tpcc {


void TPCCSwitchInfo::make_txn(const NewOrder& arg, BufferWriter& bw) {

    auto ol_cnt = arg.args.ol_cnt;
    if (ol_cnt > TPCCDeclusteredLayout::NUM_INSTRS) {
        throw std::invalid_argument("NUM_OPS != NUM_INSTR");
    }


    struct UniqInstrType_t {
        uint32_t id = 0;
        TPCCDeclusteredLayout::TupleLocation tl;
        TPCCArgs::NewOrder::OrderItem* order;

        bool operator<(const UniqInstrType_t& other) {
            if (id == other.id) {
                return tl.stage_id < other.tl.stage_id;
            }
            return id < other.id;
        }
    };

    std::array<UniqInstrType_t, TPCCDeclusteredLayout::NUM_INSTRS> accesses;
    std::array<uint32_t, TPCCDeclusteredLayout::NUM_REGS> cntr{};
    for (uint64_t i = 0; i < ol_cnt; ++i) {
        auto& order = arg.args.orders[i];
        uint64_t pk = TPCCTableInfo::Stock::pk(order.ol_supply_w_id, order.ol_i_id);
        auto& tl = declustered_layout.get_location(pk);
        uint32_t id = cntr[tl.stage_id]++;
        accesses[i] = UniqInstrType_t{id, tl, &order};
    }

    std::sort(accesses.begin(), std::next(accesses.begin(), ol_cnt));

    auto info = bw.write(info_t{});

    uint32_t nb_conflict = 0;
    lock_t locks{0, 0};
    int8_t last = -1;
    for (uint64_t i = 0; i < ol_cnt; ++i) {
        auto& tl = accesses[i].tl;
        auto& order = accesses[i].order;

        bool is_conflict = tl.stage_id <= last;
        if (tl.lock_bit == 0) { // left lock needed
            locks.left |= 1;
        } else {
            locks.right |= 1;
        }
        auto reg = InstrType_t::NO_STOCK(tl.stage_id).set_stop(is_conflict);
        bw.write(no_stock_t{reg, tl.reg_array_idx, order->ol_quantity, be_uint8_t{1}});
        nb_conflict += is_conflict;
        last = tl.stage_id;
    }

    bw.write(new_order_t{arg.get_d_id()}); // always last!
    bw.write(InstrType_t::STOP());

    if constexpr (SWITCH_NO_CONFLICT) {
        nb_conflict = 0;
    }

    info->multipass = (nb_conflict > 0);
    info->locks = (nb_conflict > 0) ? locks : lock_t{0, 0};

    // {
    //     BufferReader br{bw.buffer};
    //     auto info = br.template read<info_t>();
    //     std::cout << *info << '\n';

    //     int8_t last = -1;
    //     while (true) {
    //         auto type = br.template peek<InstrType_t>();
    //         if (*type == InstrType_t::STOP()) {
    //             break;
    //         }
    //         if (*type == InstrType_t::NEW_ORDER()) {
    //             auto new_order = br.template read<new_order_t>();
    //             std::cout << "d_id=" << new_order->d_id << " d_next_o_id=" << new_order->d_next_o_id << '\n';
    //             continue;
    //         }
    //         auto stock = br.template read<no_stock_t>();
    //         auto [is_stop, ctype] = stock->type.split_stop();
    //         std::cout << "stock[" << +ctype.reg_no_stock_idx() << "] is_stop=" << is_stop << " s_id=" << stock->s_id << " ol_quantity=" << stock->ol_quantity << " is_remote=" << stock->is_remote << '\n';

    //         if (is_stop != (ctype.value <= last)) {
    //             throw std::runtime_error("some mistake switch-txn");
    //         }
    //         last = ctype.value;
    //     }
    // }
}


void TPCCSwitchInfo::make_txn(const Payment& arg, BufferWriter& bw) {
    bw.write(info_t{});
    bw.write(payment_t{arg.get_w_id(), arg.get_d_id(), arg.get_h_amount()});
    bw.write(InstrType_t::STOP());
}

TPCCSwitchInfo::NewOrderOut TPCCSwitchInfo::parse_txn(const NewOrder& arg [[maybe_unused]], BufferReader& br) {
    br.read<info_t>();
    // while (*br.peek<InstrType_t>() != InstrType_t::NEW_ORDER()) {
    for (size_t i = 0; i < arg.args.ol_cnt; ++i) {
        br.read<no_stock_t>();
    }
    auto new_order = br.read<new_order_t>();
    // std::cout << "*new_order->d_next_o_id=" << *new_order->d_next_o_id << '\n';

    return NewOrderOut{*new_order->d_next_o_id};
}


TPCCSwitchInfo::PaymentOut TPCCSwitchInfo::parse_txn(const Payment& arg [[maybe_unused]], BufferReader& br [[maybe_unused]]) {
    return PaymentOut{};
}

} // namespace tpcc
} // namespace benchmark