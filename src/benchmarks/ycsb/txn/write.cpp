#include "../transaction.hpp"

namespace benchmark {
namespace ycsb {

YCSB::RC YCSB::operator()(YCSBArgs::Write& arg) {
    if (arg.on_switch) {
        auto write_f = atomic(p4_switch, YCSBSwitchInfo::SingleWrite{arg.id, arg.value});
        write_f->get();
        WorkerContext::get().cntr.incr(stats::Counter::ycsb_write_commits);
        return commit();
    }

    auto entry_f = write(kvs, KV::pk(arg.id));
    check(entry_f);
    auto entry = entry_f->get();
    check(entry);
    entry->value = arg.value;

    WorkerContext::get().cntr.incr(stats::Counter::ycsb_write_commits);
    return commit();
}

} // namespace ycsb
} // namespace benchmark