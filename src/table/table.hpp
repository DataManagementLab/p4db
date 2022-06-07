#pragma once

#include "comm/comm.hpp"
#include "comm/msg.hpp"
#include "concurrency_control/no_wait.hpp"
#include "concurrency_control/none.hpp"
#include "concurrency_control/wait_die.hpp"
#include "db/errors.hpp"
#include "db/mempools.hpp"
#include "db/undolog.hpp"
#include "db/util.hpp"
#include "partition.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <variant>


struct Table {
    Table() = default;
    Table(Table&&) = default;
    Table(const Table&) = delete;
    virtual ~Table() = default;

    p4db::table_t id;
    std::string name;

    // returns bytes written by tuple
    virtual size_t tuple_size() = 0;
    virtual void remote_get(Communicator::Pkt_t* pkt, msg::TupleGetReq* req) = 0;
    virtual void remote_put(msg::TuplePutReq* req) = 0;

    virtual void print(){};
};


struct Serializer {
    std::fstream file;

    Serializer(const std::string filename)
        : file(filename + ".data", std::fstream::out | std::fstream::binary) {}

    template <typename T>
    void write(const T& val) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be a POD type.");
        file.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }

    template <typename T>
    void write(const std::unique_ptr<T>& val, const size_t size) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be a POD type.");
        file.write(reinterpret_cast<const char*>(val.get()), sizeof(*val.get()) * size); // sizeof(T) Error: incomplete type
    }
};


struct DeSerializer {
    std::fstream file;

    DeSerializer(const std::string filename)
        : file(filename + ".data", std::fstream::in | std::fstream::binary) {}

    template <typename T>
    auto read() {
        static_assert(std::is_trivially_copyable<T>::value, "T must be a POD type.");
        T val{};
        file.read(reinterpret_cast<char*>(&val), sizeof(val));
        return val;
    }

    template <typename T>
    void read(const std::unique_ptr<T>& val, const size_t size) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be a POD type.");
        file.read(reinterpret_cast<char*>(val.get()), sizeof(*val.get()) * size);
    }
};

template <typename Tuple_t>
struct StructTable final : public Table {
    using Row_t = Row<Tuple_t, CC_SCHEME>;

    using Future_t = TupleFuture<Tuple_t>;


    std::atomic<uint64_t> size{0};
    const size_t max_size;

    Tuple_t::PartitionInfo_t part_info;
    Communicator& comm;
    std::unique_ptr<Row_t[]> data;
    // HugePages<Row_t> data;


    StructTable(std::size_t max_size, Communicator& comm)
        : max_size(max_size), part_info(max_size), comm(comm) {
        std::cout << "size: " << stringifyFileSize(sizeof(Row_t) * max_size) << '\n';
        data = std::make_unique<Row_t[]>(max_size);
        // data.allocate(max_size);
    }

    ~StructTable() {
        auto& config = Config::instance();
        if (!config.verify) {
            return;
        }
        for (size_t i = 0; i < max_size; ++i) {
            if (!data[i].check()) {
                std::stringstream ss;
                ss << "table: " << name << " row[" << i << "]: check failed\n";
                std::cerr << ss.str();
            }
        }
    }


    void write_dump() {
        std::cout << "write_dump() table: " << name << '\n';
        Serializer s{name};
        s.write(max_size);
        s.write(size.load());
        s.write(data, size);
    }

    bool read_dump() {
        std::cout << "read_dump() table: " << name << '\n';
        DeSerializer s{name};
        if (!s.file.good()) {
            std::cout << "file not good: " << std::strerror(errno) << '\n';
            return false;
        }
        if (s.read<uint64_t>() != max_size) {
            std::cout << "table max_size missmatch\n";
            return false;
        }
        size = s.read<size_t>();
        s.read(data, size);
        return true;
    }


    ErrorCode get(const p4db::key_t index, const AccessMode mode, Future_t* future, const timestamp_t ts) {
        auto local_index = part_info.translate(index);
        if (local_index >= size) {
            return ErrorCode::INVALID_ROW_ID;
        }

        auto& row = data[local_index];
        return row.local_lock(mode, ts, future);
    }

    ErrorCode put(p4db::key_t index, const AccessMode mode, const timestamp_t ts) {
        auto local_index = part_info.translate(index);
        if (local_index >= size) {
            return ErrorCode::INVALID_ROW_ID;
        }

        if constexpr (error::LOG_TABLE) {
            std::stringstream ss;
            ss << "local_put to " << name << " index=" << index << " local_index=" << local_index << " mode=" << mode << '\n';
            std::cout << ss.str();
        }

        auto& row = data[local_index];
        return row.local_unlock(mode, ts, comm);
    }


    auto& insert(p4db::key_t& index) {
        if (size >= max_size) {
            throw error::TableFull();
        }
        index = p4db::key_t{size++};
        return data[index].tuple;
    }


    virtual void remote_get(Communicator::Pkt_t* pkt, msg::TupleGetReq* req) override {
        auto local_index = part_info.translate(req->rid);
        auto& row = data[local_index];
        row.remote_lock(comm, pkt, req);
    }

    virtual void remote_put(msg::TuplePutReq* req) override {
        auto local_index = part_info.translate(req->rid);

        if constexpr (error::LOG_TABLE) {
            std::stringstream ss;
            ss << "remote_put to " << name << " index=" << req->rid << " local_index=" << local_index << " mode=" << req->mode << '\n';
            std::cout << ss.str();
        }


        auto& row = data[local_index];
        row.remote_unlock(req, comm);
    }

    virtual size_t tuple_size() override {
        return sizeof(Tuple_t);
    }


    class Iterator;
    Iterator begin() {
        return Iterator(data.get());
    }
    Iterator end() {
        return Iterator(&data[size]);
    }
};


template <typename Tuple_t>
class StructTable<Tuple_t>::Iterator {
    Row_t* ptr;

public:
    Iterator(Row_t* ptr) : ptr(ptr) {}

    Iterator& operator++() {
        ++ptr;
        return *this;
    }
    Iterator& operator--() {
        --ptr;
        return *this;
    }

    Tuple_t& operator*() {
        return ptr->tuple;
    }
    bool operator==(const Iterator& iter) {
        return ptr == iter.ptr;
    }
};