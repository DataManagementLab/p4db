#pragma once

#include <array>
#include <cstdint>
#include <variant>


namespace benchmark {
namespace micro_recirc {


struct MicroRecircArgs {

    struct Arg {
        uint32_t recircs = 0;
        bool on_switch = true;
    };


    using Arg_t = std::variant<Arg>;
};

} // namespace micro_recirc
} // namespace benchmark