#include "../transaction.hpp"


namespace benchmark {
namespace ycsb {

YCSB::RC YCSB::operator()(YCSBArgs::Multi<NUM_OPS>& arg) {
    if (arg.on_switch) {
        WorkerContext::get().cycl.reset(stats::Cycles::switch_txn_latency);
        WorkerContext::get().cycl.start(stats::Cycles::switch_txn_latency);
        auto multi_f = atomic(p4_switch, YCSBSwitchInfo::MultiOp{arg});
        const auto values = multi_f->get().values;
        do_not_optimize(values);

        if constexpr (!YCSB_MULTI_MIX_RW) {
            if (arg.ops[0].mode == AccessMode::WRITE) {
                WorkerContext::get().cntr.incr(stats::Counter::ycsb_write_commits);
            } else {
                WorkerContext::get().cntr.incr(stats::Counter::ycsb_read_commits);
            }
        }
        WorkerContext::get().cycl.stop(stats::Cycles::switch_txn_latency);
        WorkerContext::get().cycl.save(stats::Cycles::switch_txn_latency);
        return commit();
    }


    // acquire all locks first, ex and shared. Can rollback within loop
    TupleFuture<KV>* ops[NUM_OPS];
    for (size_t i = 0; auto& op : arg.ops) {
        if (op.mode == AccessMode::WRITE) {
            ops[i] = write(kvs, KV::pk(op.id));
        } else {
            ops[i] = read(kvs, KV::pk(op.id));
        }
        check(ops[i]);
        ++i;
    }

    // Use obtained write-locks to write values
    for (size_t i = 0; auto& op : arg.ops) {
        if (op.mode == AccessMode::WRITE) {
            auto x = ops[i]->get();
            check(x);
            x->value = op.value;
        } else {
            const auto x = ops[i]->get();
            check(x);
            const auto value = x->value;
            do_not_optimize(value);
        }
        ++i;
    }

    if constexpr (!YCSB_MULTI_MIX_RW) {
        if (arg.ops[0].mode == AccessMode::WRITE) {
            WorkerContext::get().cntr.incr(stats::Counter::ycsb_write_commits);
        } else {
            WorkerContext::get().cntr.incr(stats::Counter::ycsb_read_commits);
        }
    }

    // locks automatically released
    return commit();
}

} // namespace ycsb
} // namespace benchmark