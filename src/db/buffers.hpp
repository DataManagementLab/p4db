#pragma once

#include <cstdint>
#include <cstring>


struct BufferWriter {
    uint8_t* buffer;
    std::size_t size = 0;

    BufferWriter(uint8_t* buffer) : buffer(buffer) {}

    template <typename T>
    auto write(const T& data) {
        auto dst = reinterpret_cast<T*>(buffer + size);

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84212
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
        std::memcpy(dst, &data, sizeof(T));
#pragma GCC diagnostic pop

        size += sizeof(T);
        return dst;
    }
};

struct BufferReader {
    uint8_t* buffer;

    BufferReader(uint8_t* buffer) : buffer(buffer) {}

    template <typename T>
    auto read() {
        auto dst = reinterpret_cast<T*>(buffer);
        buffer += sizeof(T);
        return dst;
    }

    template <typename T>
    auto peek() {
        return reinterpret_cast<T*>(buffer);
    }
};