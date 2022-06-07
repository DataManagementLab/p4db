#pragma once

#include <cstdint>
#include <iostream>


namespace declustered_layout {


struct TupleLocation {
    uint8_t stage_id;
    uint8_t reg_array_id;
    uint16_t reg_array_idx;
    uint8_t lock_bit;

    friend std::ostream& operator<<(std::ostream& os, const TupleLocation& self) {
        os << "stage=" << self.stage_id << " reg=" << self.reg_array_id
           << " idx=" << self.reg_array_idx << " lock=" << self.lock_bit;
        return os;
    }
};


} // namespace declustered_layout