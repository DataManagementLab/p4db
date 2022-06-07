#include "../transaction.hpp"


namespace benchmark {
namespace ycsb {

YCSB::RC YCSB::operator()(YCSBArgs::Read& arg) {
    if (arg.on_switch) {
        auto read_f = atomic(p4_switch, YCSBSwitchInfo::SingleRead{arg.id});
        const auto value = read_f->get();
        do_not_optimize(value);
        WorkerContext::get().cntr.incr(stats::Counter::ycsb_read_commits);
        return commit();
    }

    auto entry_f = read(kvs, KV::pk(arg.id));
    check(entry_f);
    const auto entry = entry_f->get();
    check(entry);
    const auto value = entry->value;
    do_not_optimize(value);

    WorkerContext::get().cntr.incr(stats::Counter::ycsb_read_commits);
    return commit();
}

} // namespace ycsb
} // namespace benchmark