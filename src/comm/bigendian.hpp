#pragma once

#include <cstdint>
#include <iomanip>


template <typename T>
struct be_bytes_t {
private:
    T value = 0; // stored in big-endian order

    constexpr T bswap(const T bytes) const {
        switch (sizeof(T)) {
            case 1:
                return bytes;
            case 2:
                return __builtin_bswap16(bytes);
            case 4:
                return __builtin_bswap32(bytes);
            case 8:
                return __builtin_bswap64(bytes);
            default:
                throw std::invalid_argument("Unknown bswap for size=" + std::to_string(sizeof(T)));
        }
    }

public:
    be_bytes_t() = default;
    be_bytes_t(const T value) : value(bswap(value)) {}

    void operator=(const T value) {
        this->value = bswap(value);
    }

    bool operator==(const T& other) {
        return this->value == bswap(other);
    }
    bool operator==(const be_bytes_t<T>& other) {
        return this->value == other.value;
    }

    T operator*() const {
        return bswap(value);
    }

    friend std::ostream& operator<<(std::ostream& os, const be_bytes_t& self) {
        std::ios_base::fmtflags f(os.flags());
        os << "0x" << std::setfill('0') << std::setw(2 * sizeof(T)) << std::right << std::hex << +self.bswap(self.value); // + is a hack to not thread byte as char
        os.flags(f);
        return os;
    }
} __attribute__((packed));


using be_uint8_t = be_bytes_t<uint8_t>;
using be_uint16_t = be_bytes_t<uint16_t>;
using be_uint32_t = be_bytes_t<uint32_t>;
using be_uint64_t = be_bytes_t<uint64_t>;

static_assert(std::is_trivially_copyable<be_uint8_t>::value);
static_assert(std::is_trivially_copyable<be_uint16_t>::value);
static_assert(std::is_trivially_copyable<be_uint32_t>::value);
static_assert(std::is_trivially_copyable<be_uint64_t>::value);