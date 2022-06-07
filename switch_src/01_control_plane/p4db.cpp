#include <array>
#include <experimental/source_location>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <unistd.h>
#include <vector>

extern "C" {
#include <bf_pm/bf_pm_intf.h>
#include <bf_rt/bf_rt_common.h>
#include <bf_switchd/bf_switchd.h>
#include <traffic_mgr/traffic_mgr.h>
}

#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_learn.hpp>
#include <bf_rt/bf_rt_session.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <bf_rt/bf_rt_table_attributes.hpp>
#include <bf_rt/bf_rt_table_key.hpp>


#ifndef SDE_INSTALL
#error "Please add -DSDE_INSTALL=\"$SDE_INSTALL\" to CPPFLAGS"
#endif


#define INIT_STATUS_TCP_PORT 7777


#define ALL_PIPES 0xFFFF
#define BF_DEV_PIPE_PARSER_ALL 0xFF


inline void check_status(bf_status_t status, const std::experimental::source_location& location = std::experimental::source_location::current()) {
    if (status != BF_SUCCESS) {
        std::stringstream err;
        err << location.file_name() << ":" << location.line() << " got error: " << bf_err_str(status) << '\n';
        throw std::runtime_error(err.str());
    }
}

/*
 * This is the placeholder for your great NOS!
 */
int app_run(bf_switchd_context_t* switchd_ctx, const char* prog_name) {
    (void)switchd_ctx;

    bf_rt_target_t dev_tgt;
    memset(&dev_tgt, 0, sizeof(dev_tgt));
    dev_tgt.dev_id = 0;
    dev_tgt.pipe_id = BF_DEV_PIPE_ALL;


    /* Creating a new session */
    auto session = bfrt::BfRtSession::sessionCreate();
    if (session == nullptr) {
        /* Probably too many are open */
    }

    auto& dev_mgr = bfrt::BfRtDevMgr::getInstance();

    const bfrt::BfRtInfo* bfrt_info = nullptr;
    check_status(dev_mgr.bfRtInfoGet(dev_tgt.dev_id, prog_name, &bfrt_info));


    std::vector<const bfrt::BfRtTable*> table_vec;
    check_status(bfrt_info->bfrtInfoGetTables(&table_vec));

    // check_status(session->sessionCompleteOperations());


    std::cout << "Found " << table_vec.size() << " tables\n";
    for (auto& table : table_vec) {
        std::string name;
        check_status(table->tableNameGet(&name));

        size_t size;
        check_status(table->tableSizeGet(&size));

        bfrt::BfRtTable::TableType table_type;
        check_status(table->tableTypeGet(&table_type));

        std::cout << "\t" << name << " size:" << size << " type:" << static_cast<int>(table_type) << '\n';
    }


    const bfrt::BfRtTable* l2fwd_table = nullptr;
    check_status(bfrt_info->bfrtTableFromNameGet("pipe.Ingress.l2fwd", &l2fwd_table));


    bf_rt_id_t eth_dst_addr_field_id;
    check_status(l2fwd_table->keyFieldIdGet("hdr.ethernet.dst_addr", &eth_dst_addr_field_id));

    bf_rt_id_t send_action_id;
    check_status(l2fwd_table->actionIdGet("Ingress.send", &send_action_id));

    bf_rt_id_t send_port_field_id;
    check_status(l2fwd_table->dataFieldIdGet("port", send_action_id, &send_port_field_id));


    std::unique_ptr<bfrt::BfRtTableKey> key;
    check_status(l2fwd_table->keyAllocate(&key)); // allocate once, always reset

    std::unique_ptr<bfrt::BfRtTableData> data;
    check_status(l2fwd_table->dataAllocate(&data));

    struct MacPort {
        uint64_t mac;
        uint64_t port;
    };
    std::vector<MacPort> l2fwd_entries = {{}};

    check_status(session->beginBatch());

    for (auto& entry : l2fwd_entries) {
        check_status(l2fwd_table->keyReset(key.get()));
        check_status(key->setValue(eth_dst_addr_field_id, entry.mac));
        check_status(l2fwd_table->dataReset(send_action_id, data.get()));
        check_status(data->setValue(send_port_field_id, entry.port));
        check_status(l2fwd_table->tableEntryAdd(*session, dev_tgt, *key, *data));
    }
    std::cout << "Added " << l2fwd_entries.size() << " l2fwd entries\n";

    check_status(session->endBatch(true)); // hwSynchronous=true
    check_status(session->sessionCompleteOperations());


    bf_dev_id_t dev_id = 0;
    check_status(bf_pm_port_delete_all(dev_id));

    struct PortInfo {
        bf_pal_front_port_handle_t port_hdl;
        bf_port_speed_t port_speed;
    };

    std::vector<PortInfo> ports = {{}};

    std::vector<bf_dev_port_t> dev_ports;
    dev_ports.reserve(ports.size());
    for (auto& port_info : ports) {
        auto& port_hdl = port_info.port_hdl;
        auto dev_port = dev_ports.emplace_back();
        check_status(bf_pm_port_front_panel_port_to_dev_port_get(&port_hdl, &dev_id, &dev_port));
        std::cout << "conn_id=" << port_hdl.conn_id << " chnl_id=" << port_hdl.chnl_id << " --> dev_port=" << dev_port << '\n';
    }

    for (auto& port_info : ports) {
        auto& port_hdl = port_info.port_hdl;
        check_status(bf_pm_port_add(dev_id, &port_hdl, port_info.port_speed, bf_fec_type_t::BF_FEC_TYP_NONE));
        check_status(bf_pm_port_autoneg_set(dev_id, &port_hdl, port_info.autoneg));
        check_status(bf_pm_port_enable(dev_id, &port_hdl));
    }
    std::cout << "Configured " << ports.size() << " ports.\n";


    {
        std::vector<bf_pal_front_port_handle_t> port_hdls = {{}};

        std::vector<bf_dev_port_t> dev_ports;
        dev_ports.reserve(port_hdls.size());
        for (auto& port_hdl : port_hdls) {
            auto dev_port = dev_ports.emplace_back();
            check_status(bf_pm_port_front_panel_port_to_dev_port_get(&port_hdl, &dev_id, &dev_port));
            std::cout << "conn_id=" << port_hdl.conn_id << " chnl_id=" << port_hdl.chnl_id << " --> dev_port=" << dev_port << '\n';
        }

        for (auto& port_hdl : port_hdls) {
            check_status(bf_pm_port_add(dev_id, &port_hdl, bf_port_speed_t::BF_SPEED_100G, bf_fec_type_t::BF_FEC_TYP_NONE));
            check_status(bf_pm_port_autoneg_set(dev_id, &port_hdl, bf_pm_port_autoneg_policy_e::PM_AN_FORCE_DISABLE));

            check_status(bf_pm_port_loopback_mode_set(dev_id, &port_hdl, bf_loopback_mode_e::BF_LPBK_MAC_NEAR));

            // bf_loopback_mode_e mode;
            // check_status(bf_pm_port_loopback_mode_get(dev_id, &port_hdl, &mode));
            // std::cout << "\tDebug loopback_mode_get=" << static_cast<int>(mode) << '\n';

            check_status(bf_pm_pltfm_front_port_ready_for_bringup(dev_id, &port_hdl, true));

            check_status(bf_pm_port_enable(dev_id, &port_hdl));
        }
        std::cout << "Configured " << port_hdls.size() << " loopback ports.\n";

        for (auto& port : dev_ports) {
            bf_tm_ppg_hdl ppg;
            check_status(bf_tm_ppg_allocate(dev_id, port, &ppg));

            check_status(bf_tm_ppg_guaranteed_min_limit_set(dev_id, ppg, 200));

            check_status(bf_tm_ppg_app_pool_usage_set(dev_id, ppg, BF_TM_IG_APP_POOL_2, 100, BF_TM_PPG_BAF_33_PERCENT, 50));
            check_status(bf_tm_pool_size_set(dev_id, BF_TM_IG_APP_POOL_2, 0x1234));

            check_status(bf_tm_ppg_icos_mapping_set(dev_id, ppg, 0x80));

            check_status(bf_tm_ppg_lossless_treatment_enable(dev_id, ppg));

            bf_tm_queue_t queue = 7;
            check_status(bf_tm_q_guaranteed_min_limit_set(dev_id, port, queue, 250));

            check_status(bf_tm_q_app_pool_usage_set(dev_id, port, queue, BF_TM_EG_APP_POOL_3, 100, BF_TM_Q_BAF_11_PERCENT, 30));

            uint8_t q_mapping[] = {0, 2, 3, 1, 7, 4, 5, 6, 8, 9, 10, 11};
            check_status(bf_tm_port_q_mapping_set(dev_id, port, 12, q_mapping));

            check_status(bf_tm_sched_q_priority_set(dev_id, port, queue, BF_TM_SCH_PRIO_7));


            check_status(bf_tm_sched_q_dwrr_weight_set(dev_id, port, queue, 300));

            // bf_tm_sched_q_shaping_rate_set(
            //     bf_dev_id_t dev,
            //     bf_dev_port_t port,
            //     bf_tm_queue_t queue,
            //     bool pps,
            //     uint32_t burst_size,
            //     uint32_t rate)

            // bf_tm_sched_q_max_shaping_rate_enable(
            //     bf_dev_id_t dev,
            //     bf_dev_port_t port,
            //     bf_tm_queue_t queue)
            // bf_tm_sched_port_shaping_rate_set(
            //     bf_dev_id_t dev,
            //     bf_dev_port_t port,
            //     bool pps,
            //     uint32_t burst_size,
            //     uint32_t rate)
            break;
        }
    }


    /* Run Indefinitely */
    std::cout << "Press Ctrl-C to quit\n";
    while (true) {
        sleep(1);
    }
}


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <prog-name>\n";
        return -1;
    }

    const char* prog_name = argv[1];

    /* Check that we are running as root */
    if (geteuid() != 0) {
        std::cerr << "ERROR: This program must be run as root\n";
        return -1;
    }

    /* Allocate switchd context */
    bf_switchd_context_t* switchd_ctx;
    if ((switchd_ctx = (bf_switchd_context_t*)calloc(1, sizeof(bf_switchd_context_t))) == nullptr) {
        std::cerr << "Cannot Allocate switchd context\n";
        return -1;
    }

    std::string conf_file{SDE_INSTALL "/share/p4/targets/tofino/"};
    conf_file += prog_name;
    conf_file += ".conf";

    /* Minimal switchd context initialization to get things going */
    switchd_ctx->install_dir = strdup(SDE_INSTALL);
    switchd_ctx->conf_file = strdup(conf_file.c_str());
    switchd_ctx->running_in_background = true;
    switchd_ctx->dev_sts_thread = true;
    switchd_ctx->dev_sts_port = INIT_STATUS_TCP_PORT;
    // switchd_ctx->init_mode = BF_DEV_WARM_INIT_FAST_RECFG;

    /* Initialize the device */
    bf_status_t status = bf_switchd_lib_init(switchd_ctx);
    if (status != BF_SUCCESS) {
        std::cerr << "ERROR: Device initialization failed: " << bf_err_str(status) << "\n";
        return -1;
    }

    /* Run the application */
    status = app_run(switchd_ctx, prog_name);

    free(switchd_ctx);

    return status;
}