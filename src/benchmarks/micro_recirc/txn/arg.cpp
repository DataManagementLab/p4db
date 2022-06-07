#include "../transaction.hpp"

#include <stdexcept>

namespace benchmark {
namespace micro_recirc {


MicroRecirc::RC MicroRecirc::operator()(MicroRecircArgs::Arg& arg) {
    if (arg.on_switch) {
        auto txn_f = atomic(p4_switch, MicroRecircSwitchInfo::Recirc{arg.recircs});
        txn_f->get();
        return commit();
    }

    throw std::runtime_error("not implemented");
}

} // namespace micro_recirc
} // namespace benchmark