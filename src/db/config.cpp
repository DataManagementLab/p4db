#include "config.hpp"

#include <cxxopts.hpp>

class BetterParseResult : public cxxopts::ParseResult {
public:
    BetterParseResult() = delete;
    BetterParseResult(cxxopts::ParseResult&& result)
        : cxxopts::ParseResult(std::move(result)) {}

    // adds check with meaningfull error message for required options which is
    // missing in library

    template <typename T>
    const T& as(const std::string& option) const {
        if (!count(option)) {
            std::cerr << "Error: Option \"" << option << "\" is required\n";
            std::exit(EXIT_FAILURE);
        }

        const auto& value = (*this)[option];
        return value.as<T>();
    }
};

void Config::parse_cli(int argc, char** argv) {
    cxxopts::Options options("P4DB", "Database for P4 Burning Switch Project");

    // clang-format off
    options.add_options()
        ("node_id", "Server identifier, 0 indexed < num_servers", cxxopts::value<uint32_t>())
        ("num_nodes", "Number of servers to use", cxxopts::value<uint32_t>())
        ("num_txn_workers", "", cxxopts::value<uint32_t>())
        ("csv_file_cycles", "", cxxopts::value<std::string>())
        ("csv_file_periodic", "", cxxopts::value<std::string>())

        ("workload", "", cxxopts::value<BenchmarkType>())
        ("use_switch", "Whether to use switch for txn processing", cxxopts::value<bool>())
        ("verify", "Run verification, like table consistency checks for TPC-C ", cxxopts::value<bool>()->default_value("false"))
        ("num_txns", "", cxxopts::value<uint64_t>())

        ("ycsb_table_size", "", cxxopts::value<uint64_t>())
        ("ycsb_write_prob", "", cxxopts::value<int>())
        ("ycsb_remote_prob", "", cxxopts::value<int>())
        ("ycsb_hot_prob", "", cxxopts::value<int>())
        ("ycsb_hot_size", "", cxxopts::value<uint64_t>())

        ("smallbank_table_size", "", cxxopts::value<uint64_t>())
        ("smallbank_hot_prob", "", cxxopts::value<int>())
        ("smallbank_hot_size", "", cxxopts::value<uint64_t>())
        ("smallbank_remote_prob", "", cxxopts::value<int>())

        ("tpcc_num_warehouses", "", cxxopts::value<uint64_t>())
        ("tpcc_new_order_remote_prob", "access remote warehouse 1%", cxxopts::value<int>())
        ("tpcc_payment_remote_prob", "whether paying customer is from remote wh and random district 15%", cxxopts::value<int>())

        ("micro_recirc_prob", "", cxxopts::value<int>())
        
        ("h,help", "Print usage")
    ;
    // clang-format on

    BetterParseResult result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << '\n';
        std::exit(0);
    }

    node_id = result.as<uint32_t>("node_id");
    num_nodes = result.as<uint32_t>("num_nodes");
    num_txn_workers = result.as<uint32_t>("num_txn_workers");


    if (result.count("servers")) {
        servers.clear();
        // {"127.0.0.1", 4001, {}},
        servers = result.as<std::vector<Server>>("servers");
    }

    if (servers.size() < num_nodes) {
        throw std::runtime_error("Insufficient servers specified");
    }
    servers.resize(num_nodes);

    if (result.count("csv_file_cycles")) {
        if constexpr (!(ENABLED_STATS & StatsBitmask::CYCLES)) {
            throw std::runtime_error("Please compile with ENABLED_STATS|=StatsBitmask::CYCLES");
        }
        csv_file_cycles = result.as<std::string>("csv_file_cycles");
    }


    use_switch = result.as<bool>("use_switch");
    if (result.count("verify")) {
        verify = result.as<bool>("verify");
    }
    // if (use_switch) {
    switch_id = servers.size();
    servers.emplace_back(Server{"", 0, {0x1B, 0xAD, 0xC0, 0xDE, 0xBA, 0xBE}}); // switch
    // }

    num_txns = result.as<uint64_t>("num_txns");

    if (result.count("switch_entries")) {
        switch_entries = result.as<uint64_t>("switch_entries");
    }

    workload = result.as<BenchmarkType>("workload");
    switch (workload) {
        case BenchmarkType::YCSB: {
            ycsb.table_size = result.as<uint64_t>("ycsb_table_size");
            ycsb.write_prob = result.as<int>("ycsb_write_prob");
            if (!(0 <= ycsb.write_prob && ycsb.write_prob <= 100)) {
                throw std::invalid_argument("ycsb_write_prob not 0..100%");
            }
            ycsb.remote_prob = result.as<int>("ycsb_remote_prob");
            if (!(0 <= ycsb.remote_prob && ycsb.remote_prob <= 100)) {
                throw std::invalid_argument("ycsb_remote_prob not 0..100%");
            }
            if (ycsb.remote_prob > 0 && num_nodes == 1) {
                throw std::invalid_argument("ycsb.remote_prob > 0 only valid if num_nodes > 1");
            }
            ycsb.hot_prob = result.as<int>("ycsb_hot_prob");
            if (!(0 <= ycsb.hot_prob && ycsb.hot_prob <= 100)) {
                throw std::invalid_argument("ycsb_hot_prob not 0..100%");
            }
            ycsb.hot_size = result.as<uint64_t>("ycsb_hot_size");
            if (!(ycsb.hot_size < ycsb.table_size)) {
                throw std::invalid_argument("ycsb_hot_size < ycsb_table_size");
            }
            if (switch_entries > (ycsb.hot_size * num_nodes)) {
                throw std::invalid_argument("switch_entries > (ycsb.hot_size * num_nodes)");
            }
            if (!result.count("switch_entries")) {
                switch_entries = ycsb.hot_size * num_nodes;
            }

            break;
        }
        case BenchmarkType::SMALLBANK: {
            smallbank.table_size = result.as<uint64_t>("smallbank_table_size");
            smallbank.hot_prob = result.as<int>("smallbank_hot_prob");
            if (!(0 <= smallbank.hot_prob && smallbank.hot_prob <= 100)) {
                throw std::invalid_argument("smallbank_hot_prob not 0..100%");
            }
            smallbank.hot_size = result.as<uint64_t>("smallbank_hot_size");
            if (!(smallbank.hot_size < smallbank.table_size)) {
                throw std::invalid_argument("smallbank_hot_size < smallbank_table_size");
            }
            smallbank.remote_prob = result.as<int>("smallbank_remote_prob");
            if (!(0 <= smallbank.remote_prob && smallbank.remote_prob <= 100)) {
                throw std::invalid_argument("smallbank_remote_prob not 0..100%");
            }
            if (smallbank.remote_prob > 0 && num_nodes == 1) {
                throw std::invalid_argument("smallbank.remote_prob > 0 only valid if num_nodes > 1");
            }
            smallbank.hot_prob = result.as<int>("smallbank_hot_prob");
            if (!(0 <= smallbank.hot_prob && smallbank.hot_prob <= 100)) {
                throw std::invalid_argument("smallbank_hot_prob not 0..100%");
            }
            break;
        }
        case BenchmarkType::TPCC: {
            tpcc.num_warehouses = result.as<uint64_t>("tpcc_num_warehouses");
            /*if (tpcc.num_warehouses == 1) {
                tpcc.home_w_id = 0;
            } else if (tpcc.num_warehouses == num_nodes) {
            } else {
                throw std::invalid_argument("tpcc_num_warehouses needs to be 1 or num_nodes.");
            }*/
            if (tpcc.num_warehouses % num_nodes != 0) {
                throw std::invalid_argument("tpcc_num_warehouses % num_nodes != 0");
            }
            tpcc.home_w_id = node_id * tpcc.num_warehouses / num_nodes;
            tpcc.num_districts = tpcc.num_warehouses * DISTRICTS_PER_WAREHOUSE;
            if (result.count("tpcc_new_order_remote_prob")) {
                tpcc.new_order_remote_prob = result.as<int>("tpcc_new_order_remote_prob");
                if (!(0 <= tpcc.new_order_remote_prob && tpcc.new_order_remote_prob <= 100)) {
                    throw std::invalid_argument("tpcc.new_order_remote_prob not 0..100%");
                }
            }
            if (result.count("tpcc_payment_remote_prob")) {
                tpcc.payment_remote_prob = result.as<int>("tpcc_payment_remote_prob");
                if (!(0 <= tpcc.payment_remote_prob && tpcc.payment_remote_prob <= 100)) {
                    throw std::invalid_argument("tpcc.payment_remote_prob not 0..100%");
                }
            }
            if (!result.count("switch_entries")) {
                switch_entries = 10 * (65536 / 2 + 32768 / 4); // Maximum default
            }

            break;
        }
        case BenchmarkType::MICRO_RECIRC: {
            micro_recirc.recirc_prob = result.as<int>("micro_recirc_prob");
            if (!(0 <= micro_recirc.recirc_prob && micro_recirc.recirc_prob <= 100)) {
                throw std::invalid_argument("micro_recirc_prob not 0..100%");
            }
            if (!use_switch) {
                throw std::invalid_argument("use_switch not true");
            }
            break;
        }
    }

    if (!use_switch) {
        switch_entries = 0;
    }
}

void Config::print() {
    std::stringstream ss;
    ss << std::boolalpha;
    ss << "node_id=" << node_id << '\n';
    ss << "num_nodes=" << num_nodes << '\n';
    ss << "num_txn_workers=" << num_txn_workers << '\n';
    ss << "num_txns=" << num_txns << '\n';
    ss << "csv_file_cycles=" << csv_file_cycles << '\n';
    ss << "cc_scheme=" << CC_SCHEME << '\n';
    ss << "use_switch=" << use_switch << '\n';
    ss << "switch_no_conflict=" << SWITCH_NO_CONFLICT << '\n';
    ss << "verify=" << verify << '\n';
    ss << "lm_on_switch=" << LM_ON_SWITCH << '\n';
    ss << "switch_entries=" << switch_entries << '\n';

    switch (workload) {
        case BenchmarkType::YCSB: {
            ss << "workload=ycsb\n";
            ss << "ycsb_table_size=" << ycsb.table_size << '\n';
            ss << "ycsb_write_prob=" << ycsb.write_prob << '\n';
            ss << "ycsb_remote_prob=" << ycsb.remote_prob << '\n';
            ss << "ycsb_hot_size=" << ycsb.hot_size << '\n';
            ss << "ycsb_hot_prob=" << ycsb.hot_prob << '\n';
            break;
        }
        case BenchmarkType::TPCC: {
            ss << "workload=tpcc\n";
            ss << "tpcc_num_warehouses=" << tpcc.num_warehouses << '\n';
            ss << "tpcc_num_districts=" << tpcc.num_districts << '\n';
            ss << "tpcc_home_w_id=" << tpcc.home_w_id << '\n';
            ss << "tpcc_new_order_remote_prob=" << tpcc.new_order_remote_prob << '\n';
            ss << "tpcc_payment_remote_prob=" << tpcc.payment_remote_prob << '\n';
            break;
        }
        case BenchmarkType::SMALLBANK: {
            ss << "workload=smallbank\n";
            ss << "smallbank_table_size=" << smallbank.table_size << '\n';
            ss << "smallbank_hot_prob=" << smallbank.hot_prob << '\n';
            ss << "smallbank_hot_size=" << smallbank.hot_size << '\n';
            ss << "smallbank_remote_prob=" << smallbank.remote_prob << '\n';
            break;
        }
        case BenchmarkType::MICRO_RECIRC: {
            ss << "workload=micro_recirc\n";
            ss << "micro_recirc_prob=" << micro_recirc.recirc_prob << '\n';
            break;
        }
    }

    std::cout << ss.str();
}