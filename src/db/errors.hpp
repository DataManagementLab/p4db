#pragma once

#include <exception>


namespace error {


struct TableFull : public std::exception {
    const char* what() const noexcept {
        return "Table is full, allocate more memory";
    }
};
struct TableCastFailed : public std::exception {
    const char* what() const noexcept {
        return "Could not dynamic_cast table to type";
    }
};
struct InvalidAccessMode : public std::exception {
    const char* what() const noexcept {
        return "Invalid Access mode requested";
    }
};
struct FutureException : public std::exception {
    const char* what() const noexcept {
        return "Exception within Future";
    }
};
struct UndoException : public std::exception {
    const char* what() const noexcept {
        return "Exception within Undo";
    }
};
struct PacketBufferTooSmall : public std::exception {
    const char* what() const noexcept {
        return "PacketBuffer too small";
    }
};
struct SerializerReadCmp : public std::exception {
    const char* what() const noexcept {
        return "SerializerReadCmp got wrong value";
    }
};


} // namespace error


enum class [[nodiscard]] ErrorCode{
    SUCCESS = 0,

    READ_LOCK_FAILED,
    WRITE_LOCK_FAILED,
    INVALID_ROW_ID,
    INVALID_ACCESS_MODE,
};

inline bool operator!(ErrorCode e) {
    return e != ErrorCode::SUCCESS;
}