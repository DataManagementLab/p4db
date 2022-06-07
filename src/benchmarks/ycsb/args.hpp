#pragma once

#include "db/defs.hpp"
#include "db/types.hpp"

#include <array>
#include <cstdint>
#include <variant>


namespace benchmark {
namespace ycsb {

struct YCSBArgs {
    struct Write {
        uint64_t id;
        uint32_t value;
        bool on_switch;
        bool is_hot;
    };

    struct Read {
        uint64_t id;
        bool on_switch;
        bool is_hot;
    };

    template <std::size_t N>
    struct Multi {
        struct OP {
            uint64_t id;
            AccessMode mode;
            uint32_t value;
        };
        std::array<OP, N> ops;
        bool on_switch;
        bool is_hot;
    };


    using Arg_t = std::variant<Write, Read, Multi<NUM_OPS>>;
};

} // namespace ycsb
} // namespace benchmark