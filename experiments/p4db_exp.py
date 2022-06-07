import re
import os
from distexprunner import *
import config


PROJECT_DIR = '/home/mjasny/burning_switch'
RUNS = 3
ENV = {
    'LD_LIBRARY_PATH': '/usr/local/lib/x86_64-linux-gnu/'
}
TIMEOUT = 300

# servers = config.server_list[lambda s: s.id.startswith('node'), lambda s: s.id == 'switch'][:2]
servers = config.server_list[lambda s: s.id.startswith('node')][:8]


def scale(vals, factor):
    return list(x*factor for x in vals)


CONFIG_FILE = './src/db/defs.hpp'
CONFIG_REGEX = r'^[ ]*constexpr\s+(?P<type>[a-zA-Z]\w+)\s+(?P<identifier>[a-zA-Z]\w+)\s+=\s+(?P<value>.+?);'


def compile(servers, clean_build=True, _id=None):
    servers.cd(PROJECT_DIR)

    kill_cmd = f'sudo killall p4db; :'
    procs = [s.run_cmd(kill_cmd)
             for s in servers[lambda s: s.id.startswith('node')]]
    assert(all(p.wait() == 0 for p in procs))

    env = {
        'CC': 'gcc-10',
        'CXX': 'g++-10',
    }

    if clean_build:
        rm_cmd = f'rm -rf build_release/'
        procs = [s.run_cmd(rm_cmd) for s in servers]
        assert(all(p.wait() == 0 for p in procs))

        meson_cmd = f'meson build_release/'
        procs = [s.run_cmd(meson_cmd, env=env) for s in servers]
        assert(all(p.wait() == 0 for p in procs))

    ninja_cmd = f'ninja -v -C build_release/'
    procs = [s.run_cmd(ninja_cmd) for s in servers]
    assert(all(p.wait() == 0 for p in procs))


def set_config(servers, override={}, _id=None):
    servers.cd(PROJECT_DIR)

    def repl(m):
        start = m.start('value') - m.start()
        end = m.end('value') - m.start()
        value = override.get(m.group('identifier'), m.group('value'))
        return m.group()[:start] + value + m.group()[end:]

    base_config = open(os.path.join(PROJECT_DIR, CONFIG_FILE)).read()
    new_config = re.sub(CONFIG_REGEX, repl, base_config, flags=re.MULTILINE)

    cmd = f'cat > {CONFIG_FILE}'
    procs = [s.run_cmd(cmd) for s in servers]
    [p.stdin(new_config, close=True) for p in procs]
    assert(all(p.wait() == 0 for p in procs))


def start_firmware(servers, firmware, _id=None):
    # tmux display-message -p '#S:#I.#{pane_index}'
    TMUX_SESSION_ID = 'main_p4db:0.1'
    TMUX_CTRL_C = f'tmux send-keys -t {TMUX_SESSION_ID} C-c'

    def TMUX_CMD(cmd):
        return f'tmux send-keys -t {TMUX_SESSION_ID} "{cmd}" ENTER'

    switch = servers['switch']

    assert(switch.run_cmd(TMUX_CTRL_C).wait() == 0)
    assert(switch.run_cmd(
        TMUX_CMD(f'make && sudo ./p4db {firmware}')).wait() == 0)

    sleep(5)  # wait until FW is loaded


expid = counter(0)


def setup(servers, override, firmware=None):
    reg_exp(servers=servers, params=ParameterGrid(
        _id=next(expid),
        override=override
    ))(set_config)
    reg_exp(servers=servers, params=ParameterGrid(
        _id=next(expid),
        clean_build=True
    ))(compile)

    if firmware is not None:
        reg_exp(servers=config.server_list, params=ParameterGrid(
            _id=next(expid),
            firmware=firmware
        ))(start_firmware)


# @reg_exp(servers=servers, params=params, raise_on_rc=False, max_restarts=0)
def p4db(servers, csv_file, num_nodes, _run=None, _id=None, **kwargs):
    servers.cd(f'{PROJECT_DIR}/build_release')

    # if kwargs.get('use_switch', True):
    #     return

    kill_cmd = f'sudo killall p4db; :'
    procs = [s.run_cmd(kill_cmd)
             for s in servers[lambda s: s.id.startswith('node')]]
    assert(all(p.wait() == 0 for p in procs))

    ids = counter(0)
    csvs = IterClassGen(CSVGenerator, [
        r'node_id=(?P<node_id>\d+)',
        r'num_nodes=(?P<num_nodes>\d+)',
        r'num_txn_workers=(?P<num_txn_workers>\d+)',
        r'num_txns=(?P<num_txns>\d+)',
        r'cc_scheme=(?P<cc_scheme>\w+)',
        r'use_switch=(?P<use_switch>(true|false))',
        r'switch_no_conflict=(?P<switch_no_conflict>(true|false))',
        # r'ycsb_opti_test=(?P<ycsb_opti_test>(true|false))',
        r'lm_on_switch=(?P<lm_on_switch>(true|false))',
        r'csv_file_cycles=(?P<csv_file_cycles>[\w\.\/]+)',
        # r'csv_file_periodic=(?P<csv_file_periodic>[\w\.\/]+)',

        r'workload=(?P<workload>\w+)',

        # hack to have optional fields
        CSVGenerator.Sum(r'ycsb_table_size=(?P<ycsb_table_size>\d+)'),
        CSVGenerator.Sum(r'ycsb_write_prob=(?P<ycsb_write_prob>\d+)'),
        CSVGenerator.Sum(r'ycsb_hot_size=(?P<ycsb_hot_size>\d+)'),
        CSVGenerator.Sum(r'ycsb_hot_prob=(?P<ycsb_hot_prob>\d+)'),
        CSVGenerator.Sum(r'ycsb_remote_prob=(?P<ycsb_remote_prob>\d+)'),

        CSVGenerator.Sum(r'tpcc_num_warehouses=(?P<tpcc_num_warehouses>\d+)'),
        CSVGenerator.Sum(r'tpcc_num_districts=(?P<tpcc_num_districts>\d+)'),
        CSVGenerator.Sum(r'tpcc_home_w_id=(?P<tpcc_home_w_id>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_new_order_remote_prob=(?P<new_order_remote_prob>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_payment_remote_prob=(?P<payment_remote_prob>\d+)'),
        CSVGenerator.Sum(
            r'switch_entries=(?P<switch_entries>\d+)'),

        CSVGenerator.Sum(
            r'smallbank_table_size=(?P<smallbank_table_size>\d+)'),
        CSVGenerator.Sum(r'smallbank_hot_prob=(?P<smallbank_hot_prob>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_remote_prob=(?P<smallbank_remote_prob>\d+)'),
        CSVGenerator.Sum(r'smallbank_hot_size=(?P<smallbank_hot_size>\d+)'),

        CSVGenerator.Sum(r'micro_recirc_prob=(?P<micro_recirc_prob>\d+)'),


        # STATS_ENABLE = true
        CSVGenerator.Sum(
            r'local_read_lock_failed=(?P<local_read_lock_failed>\d+)'),
        CSVGenerator.Sum(
            r'local_write_lock_failed=(?P<local_write_lock_failed>\d+)'),
        CSVGenerator.Sum(r'local_lock_success=(?P<local_lock_success>\d+)'),
        CSVGenerator.Sum(r'local_lock_waiting=(?P<local_lock_waiting>\d+)'),
        CSVGenerator.Sum(r'remote_lock_failed=(?P<remote_lock_failed>\d+)'),
        CSVGenerator.Sum(r'remote_lock_success=(?P<remote_lock_success>\d+)'),
        CSVGenerator.Sum(r'remote_lock_waiting=(?P<remote_lock_waiting>\d+)'),
        CSVGenerator.Sum(r'switch_aborts=(?P<switch_aborts>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_warehouse_read=(?P<tpcc_no_warehouse_read>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_district_write=(?P<tpcc_no_district_write>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_customer_read=(?P<tpcc_no_customer_read>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_warehouse_f_get=(?P<tpcc_no_warehouse_f_get>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_district_f_get=(?P<tpcc_no_district_f_get>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_customer_f_get=(?P<tpcc_no_customer_f_get>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_stock_acquired=(?P<tpcc_no_stock_acquired>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_no_item_acquired=(?P<tpcc_no_item_acquired>\d+)'),
        CSVGenerator.Sum(r'tpcc_no_commits=(?P<tpcc_no_commits>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_warehouse_write=(?P<tpcc_pay_warehouse_write>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_district_write=(?P<tpcc_pay_district_write>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_customer_write=(?P<tpcc_pay_customer_write>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_warehouse_f_get=(?P<tpcc_pay_warehouse_f_get>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_district_f_get=(?P<tpcc_pay_district_f_get>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_customer_f_get=(?P<tpcc_pay_customer_f_get>\d+)'),
        CSVGenerator.Sum(r'tpcc_pay_commits=(?P<tpcc_pay_commits>\d+)'),
        CSVGenerator.Sum(
            r'micro_recirc_recircs=(?P<micro_recirc_recircs>\d+)'),
        CSVGenerator.Sum(r'ycsb_read_commits=(?P<ycsb_read_commits>\d+)'),
        CSVGenerator.Sum(r'ycsb_write_commits=(?P<ycsb_write_commits>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_amalgamate_commits=(?P<smallbank_amalgamate_commits>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_balance_commits=(?P<smallbank_balance_commits>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_deposit_checking_commits=(?P<smallbank_deposit_checking_commits>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_send_payment_commits=(?P<smallbank_send_payment_commits>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_write_check_commits=(?P<smallbank_write_check_commits>\d+)'),
        CSVGenerator.Sum(
            r'smallbank_transact_saving_commits=(?P<smallbank_transact_saving_commits>\d+)'),

        CSVGenerator.Sum(r'tpcc_no_txns=(?P<tpcc_no_txns>\d+)'),
        CSVGenerator.Sum(r'tpcc_pay_txns=(?P<tpcc_pay_txns>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_warehouse_read=(?P<tpcc_pay_warehouse_read>\d+)'),
        CSVGenerator.Sum(
            r'tpcc_pay_district_read=(?P<tpcc_pay_district_read>\d+)'),

        # Cycle stats
        CSVGenerator.Sum(r'commit_latency=(?P<commit_latency>.+)$'),
        CSVGenerator.Sum(r'latch_contention=(?P<latch_contention>.+)$'),
        CSVGenerator.Sum(r'remote_latency=(?P<remote_latency>.+)$'),
        CSVGenerator.Sum(r'local_latency=(?P<local_latency>.+)$'),
        CSVGenerator.Sum(r'switch_txn_latency=(?P<switch_txn_latency>.+)$'),


        # summary stats
        r'total_tps=(?P<total_tps>\d+)',
        r'total_cps=(?P<total_cps>\d+)',
        r'total_commits=(?P<total_commits>\d+)',
        r'total_aborts=(?P<total_aborts>\d+)',
        r'total_txns=(?P<total_txns>\d+)',
        r'total_on_switch=(?P<total_on_switch>\d+)',
        r'avg_duration=(?P<avg_duration>\d+)',
    ], run=_run,
    )

    procs = []
    for s in servers[:num_nodes]:
        node_id = next(ids)
        env_hack = ' '.join(f'{k}={v}' for k, v in ENV.items())
        cmd = [
            f'sudo {env_hack} ./p4db',
            f'--node_id={node_id} --num_nodes={num_nodes}'
        ]
        # automatic generate program arguments
        cmd.extend(f'--{k}={v}' for k, v in kwargs.items())

        proc = s.run_cmd(' '.join(cmd), stdout=next(
            csvs), env=ENV, timeout=TIMEOUT)
        procs.append(proc)

    rc = any_failed(procs)
    if rc == ReturnCode.TIMEOUT:
        return Action.RESTART
    if rc is not False:
        sleep(5)
        return Action.RESTART
    assert(rc == 0)

    for i, csv in enumerate(csvs):
        if i == 0:
            log(csv.header)
        log(csv.row)
        csv.write(csv_file)


def ycsb_a(servers):
    def exp_a():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_a.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[0, 5, 50],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[75],
            use_switch=[False, True]
        )

    # setup(servers, firmware='p4db_ycsb', override={
    #     'CC_SCHEME': 'CC_Scheme::NO_WAIT',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'ENABLED_STATS': 'StatsBitmask::NONE',
    #     'LM_ON_SWITCH': 'false'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_a())(p4db)

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_a())(p4db)

    # LM on switch test
    # def exp_a2():
    #     return ParameterGrid(
    #         _id=next(expid),
    #         _run=range(RUNS),
    #         csv_file='exp_a.csv',
    #         num_nodes=[8],
    #         workload=['ycsb'],
    #         ycsb_write_prob=[0, 5, 50],
    #         num_txn_workers=[20],
    #         num_txns=[500_000],

    #         ycsb_remote_prob=[20],
    #         ycsb_table_size=[10_000_000],
    #         ycsb_hot_size=[50],
    #         ycsb_hot_prob=[75],
    #         use_switch=False
    #     )
    # setup(servers, firmware='p4db_lock_manager', override={
    #     'CC_SCHEME': 'CC_Scheme::NO_WAIT',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'ENABLED_STATS': 'StatsBitmask::NONE',
    #     'LM_ON_SWITCH': 'true',
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_a2())(p4db)

    # setup(servers, firmware='p4db_lock_manager', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'ENABLED_STATS': 'StatsBitmask::NONE',
    #     'LM_ON_SWITCH': 'true',
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_a2())(p4db)


def ycsb_b(servers):
    # def exp_b():
    #     return ParameterGrid(
    #         _id=next(expid),
    #         _run=range(RUNS),
    #         csv_file='exp_b.csv',
    #         num_nodes=[8],
    #         workload=['ycsb'],
    #         ycsb_write_prob=[0, 5, 50],
    #         num_txn_workers=[20],
    #         num_txns=[500_000],

    #         ycsb_remote_prob=[10, 20, 30, 40, 50, 60, 70, 80, 90],
    #         ycsb_table_size=[10_000_000],
    #         ycsb_hot_size=[50],
    #         ycsb_hot_prob=[75],
    #         use_switch=[False, True]
    #     )

    # setup(servers, firmware='p4db_ycsb', override={
    #     'CC_SCHEME': 'CC_Scheme::NO_WAIT',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'StatsBitmask::NONE'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_b())(p4db)

    # setup(servers, firmware='p4db_ycsb', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'StatsBitmask::NONE'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_b())(p4db)

    def exp_b2():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_b.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[0, 5, 50],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[10, 20, 30, 40, 50, 60, 70, 80, 90],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[75],
            use_switch=False
        )

    setup(servers, firmware='p4db_lock_manager', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_b2())(p4db)

    setup(servers, firmware='p4db_lock_manager', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_b2())(p4db)


def ycsb_c(servers):
    # def exp_c():
    #     return ParameterGrid(
    #         _id=next(expid),
    #         _run=range(RUNS),
    #         csv_file='exp_c.csv',
    #         num_nodes=[8],
    #         workload=['ycsb'],
    #         ycsb_write_prob=[0, 5, 50],
    #         num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
    #         num_txns=[500_000],

    #         ycsb_remote_prob=[20],
    #         ycsb_table_size=[10_000_000],
    #         ycsb_hot_size=[50],
    #         ycsb_hot_prob=[75],
    #         use_switch=[False, True]
    #     )

    # setup(servers, firmware='p4db_ycsb', override={
    #     'CC_SCHEME': 'CC_Scheme::NO_WAIT',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'StatsBitmask::NONE'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_c())(p4db)

    # setup(servers, firmware='p4db_ycsb', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'StatsBitmask::NONE'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_c())(p4db)

    def exp_c2():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_c.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[0, 5, 50],
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[75],
            use_switch=False
        )

    setup(servers, firmware='p4db_lock_manager', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_c2())(p4db)

    setup(servers, firmware='p4db_lock_manager', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_c2())(p4db)


def ycsb_d(servers):
    def exp_d():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_d.csv',
            num_nodes=[8],
            workload=['ycsb'],
            # ycsb_write_prob=[0, 5, 50],
            ycsb_write_prob=[50],
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[75],
            use_switch=[True]
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_d())(p4db)

    # setup(servers, firmware='p4db_ycsb', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'false',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'false'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_d())(p4db)


def ycsb_skew(servers):
    def exp_skew():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_ycsb_skew.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[50],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)


def ycsb_skew2(servers):
    def exp_skew():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_ycsb_skew2.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[50],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 800, 900, 1000,
                           1500, 2000, 3000, 4000, 5000, 10000, 20000, 30000, 40000, 50000, 100000],
            ycsb_hot_prob=[75],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)


def ycsb_cycles(servers):
    cycles_csv_id = counter()
    periodic_csv_id = counter()

    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_cycles.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[0, 5, 50],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[75],
            use_switch=[False, True],
            csv_file_cycles=ComputedParam(
                lambda _id: ['../cycles2_{_id}.csv'.format(_id=next(cycles_csv_id))]),
            csv_file_periodic=ComputedParam(
                lambda _id: ['../periodic2_{_id}.csv'.format(_id=next(periodic_csv_id))])
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def ycsb_optis(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_ycsb_optis.csv',
            num_nodes=[4],
            workload=['ycsb'],
            ycsb_write_prob=[50],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[100],
            use_switch=[True]
        )

    setup(servers, override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'YCSB_OPTI_TEST': 'true',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)

    setup(servers, override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'YCSB_OPTI_TEST': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)

    setup(servers, override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'YCSB_OPTI_TEST': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def smallbank_e(servers):
    def exp_e():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_e.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_table_size=[1_000_000],
            smallbank_hot_size=[5, 10, 15],
            smallbank_hot_prob=[90],  # , 50],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'LM_ON_SWITCH': 'false',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_e())(p4db)

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'LM_ON_SWITCH': 'false',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_e())(p4db)


def smallbank_f(servers):
    def exp_f():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_f.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[10, 20, 30, 40, 50, 60, 70, 80, 90],
            smallbank_table_size=[1_000_000],
            smallbank_hot_size=[5, 10, 15],
            smallbank_hot_prob=[90],  # , 50],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'LM_ON_SWITCH': 'false',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_f())(p4db)

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'LM_ON_SWITCH': 'false',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_f())(p4db)


def smallbank_g(servers):
    def exp_g():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_g.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_table_size=[1_000_000],
            smallbank_hot_size=[5, 10, 15],
            smallbank_hot_prob=[90],  # , 50],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'LM_ON_SWITCH': 'false',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_g())(p4db)

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'LM_ON_SWITCH': 'false',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_g())(p4db)


def smallbank_h(servers):
    def exp_h():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_h.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_table_size=[1_000_000],
            # smallbank_hot_size=[5, 10, 15],
            smallbank_hot_size=[5],
            smallbank_hot_prob=[90],  # , 50],
            use_switch=[True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_h())(p4db)

    # setup(servers, firmware='p4db_smallbank', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'false',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'false'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_h())(p4db)


def smallbank_skew(servers):
    def exp_skew():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_smallbank_skew.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_table_size=[1_000_000],
            smallbank_hot_size=[5],
            smallbank_hot_prob=[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)


def smallbank_skew2(servers):
    def exp_skew():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_smallbank_skew2.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_table_size=[1_000_000],
            # smallbank_hot_size=[5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 35, 40, 45, 50, 100, 200, 300, 400, 500,
            #                    600, 800, 900, 1000, 1500, 2000, 3000, 4000, 5000, 10000, 20000, 30000, 40000, 50000],  # , 100000],
            smallbank_hot_size=[5, 6, 7, 8, 9, 10,
                                15, 20, 25, 30, 35, 40, 45, 50],
            smallbank_hot_prob=[90],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_skew())(p4db)


# WARNING changed remote_prob for i, k, l


def tpcc_i(servers):
    def exp_i():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_i.csv',
            num_nodes=[8],
            workload=['tpcc'],
            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            num_txn_workers=[20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[20],  # 20 normal
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_i())(p4db)

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_i())(p4db)


def tpcc_j(servers):
    def exp_j():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_j.csv',
            num_nodes=[8],
            workload=['tpcc'],
            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            num_txn_workers=[20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[10, 20, 30, 40, 50, 60, 70, 80, 90],
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_j())(p4db)

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_j())(p4db)


def tpcc_k(servers):
    def exp_k():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_k.csv',
            num_nodes=[8],
            workload=['tpcc'],
            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[20],  # 20 normal
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_k())(p4db)

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_k())(p4db)


def tpcc_l(servers):
    def exp_l():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_l.csv',
            num_nodes=[8],
            workload=['tpcc'],
            # tpcc_num_warehouses=ComputedParam(
            #   lambda num_nodes: scale([1, 2, 4], num_nodes)),
            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1], num_nodes)),
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[20],
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[True]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp_l())(p4db)

    # setup(servers, firmware='p4db_tpcc', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'false',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'StatsBitmask::NONE',
    #     'ORDER_CNT_MAX': '10'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp_l())(p4db)


def part_comp(servers):
    cycles_csv_id = counter()

    def ycsb():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_partcomp_cycles.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[50],
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50],
            ycsb_hot_prob=[75],
            use_switch=[True],
            csv_file_cycles=ComputedParam(
                lambda _id: ['../cycles_{_id}.csv'.format(_id=next(cycles_csv_id))])
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=ycsb())(p4db)

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=ycsb())(p4db)

    # while next(cycles_csv_id) < 47:
    #     pass

    def smallbank():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_partcomp_cycles.csv',
            num_nodes=[8],
            workload=['smallbank'],
            smallbank_hot_size=[5],
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_table_size=[1_000_000],
            smallbank_hot_prob=[90],
            use_switch=[True],
            csv_file_cycles=ComputedParam(
                lambda _id: ['../cycles_{_id}.csv'.format(_id=next(cycles_csv_id))])
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=smallbank())(p4db)

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=smallbank())(p4db)

    def tpcc():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_partcomp_cycles.csv',
            num_nodes=[8],
            workload=['tpcc'],

            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1], num_nodes)),
            num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[20],
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[True],
            csv_file_cycles=ComputedParam(
                lambda _id: ['../cycles_{_id}.csv'.format(_id=next(cycles_csv_id))])
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
        'ORDER_CNT_MAX': '10'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=tpcc())(p4db)

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'false',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::ALL',
        'ORDER_CNT_MAX': '10'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=tpcc())(p4db)


def ycsb_hotcold(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_ycsb_hotcold.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[50, 5, 0],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            # ycsb_table_size=[10_000_000],
            ycsb_hot_size=[50, 75, 100, 1_000, 10_000, 100_000],
            ycsb_table_size=ComputedParam(
                lambda ycsb_hot_size, num_nodes: int(num_nodes*ycsb_hot_size*2 * 1.1)),  # *2 for rounding error?
            # Simulate contention on cold part, e.g. hot-set lays on nodes
            # ycsb_hot_prob=[75],
            ycsb_hot_prob=[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::NONE',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def ycsb_overhot(servers):
    # cat exp_ycsb_overhot.csv | awk -F',' '{row[substr($0, 0, 100)]=$0} END {for (i in row) print row[i] }' | sort -r > exp_ycsb_overhot_fixed.csv

    import numpy as np
    CAPACITIES = [1000, 10000, 65536, 655360]
    # Switch Max-YCSB Capacity: 655360

    def exp():
        def switch_entries(use_switch, num_nodes, ycsb_hot_size):
            if not use_switch:
                return [0]
            return set(min(num_nodes*ycsb_hot_size, c) for c in CAPACITIES)
            # return set([
            #     # +[110, 120, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 130, 140, 150]
            #     num_nodes*min(ycsb_hot_size, 125),
            #     # + [1000, 1200, 1250, 1300, 1400, 1500, 1700, 2000, 3000, 4000, 5000]
            #     num_nodes*min(ycsb_hot_size, 1250),
            #     # + [9000, 12000, 16000, 25000, 35000]
            #     num_nodes*min(ycsb_hot_size, 8192),
            #     # + [150000, 200000, 300000]
            #     num_nodes*min(ycsb_hot_size, 81920),
            # ])

        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_ycsb_overhot2.csv',
            num_nodes=[8],
            workload=['ycsb'],
            ycsb_write_prob=[50],  # , 5, 0],
            num_txn_workers=[20],
            num_txns=[500_000],

            ycsb_remote_prob=[20],
            ycsb_hot_size=(np.array(CAPACITIES)/8+1).astype(int).tolist() + np.ceil(np.logspace(
                np.log10(400/8), np.log10(8_000_000/8), 50, endpoint=True)).astype(int).tolist(),

            # [50, 75, 100, 200, 300, 400, 500, 1_000, 1_250, 1_251, 2_500, 5_000, 8_192, 8_193, 10_000,
            # 50_000, 80_192, 80_193, 100_000, 500_000, 1_000_000],
            ycsb_table_size=[10_000_000],
            ycsb_hot_prob=[75],  # , 90],
            use_switch=[True, False],  # [True, False],
            switch_entries=ComputedParam(switch_entries),
        )

    setup(servers, firmware='p4db_ycsb', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def smallbank_hotcold(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_smallbank_hotcold.csv',
            num_nodes=[8],
            workload=['smallbank'],
            num_txn_workers=[20],
            num_txns=[1_000_000],

            smallbank_remote_prob=[20],
            smallbank_hot_size=[5, 10, 15, 20, 25,
                                50, 100, 1_000, 10_000, 100_000],
            smallbank_table_size=ComputedParam(
                lambda smallbank_hot_size, num_nodes: int(num_nodes*smallbank_hot_size*2 * 1.1)),  # *2 for rounding error?
            smallbank_hot_prob=[0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_smallbank', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'LM_ON_SWITCH': 'false'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def tpcc_hotsize(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_tpcc_hotsize.csv',
            num_nodes=[8],
            workload=['tpcc'],
            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            num_txn_workers=[20],
            num_txns=[500_000],
            switch_entries=[0, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536,
                            131072, 262144, 409600],  # max= 10 * (65536 / 2 + 32768 / 4)

            tpcc_new_order_remote_prob=[20],  # 20 normal
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def tpcc_chiller(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_tpcc_chiller.csv',
            num_nodes=[8],
            workload=['tpcc'],
            num_txn_workers=[20],  # [50, 26, 20],
            num_txns=[200_000],

            switch_entries=[int(65536/16)],  # 409600],
            tpcc_new_order_remote_prob=[20, 80],  # 20 normal
            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'ENABLED_STATS': 'StatsBitmask::NONE',
        'ORDER_CNT_MAX': '10',
        'LM_ON_SWITCH': 'false',
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)

    # setup(servers, firmware='p4db_tpcc', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'ENABLED_STATS': 'StatsBitmask::NONE',
    #     'ORDER_CNT_MAX': '10',
    #     'LM_ON_SWITCH': 'false',
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp())(p4db)


def tpcc_aborts(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_tpcc_aborts2.csv',
            num_nodes=[8],
            workload=['tpcc'],

            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            num_txn_workers=[8, 20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[20, 80],
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True]
        )
        # def remote_prob(num_txn_workers):
        #     # if num_txn_workers != 20:
        #     #     return [20]
        #     return [10, 20, 30, 40, 50, 60, 70, 80, 90]

        # return ParameterGrid(
        #     _id=next(expid),
        #     _run=range(RUNS),
        #     csv_file='exp_tpcc_aborts.csv',
        #     num_nodes=[8],
        #     workload=['tpcc'],

        #     tpcc_num_warehouses=ComputedParam(
        #         lambda num_nodes: scale([1], num_nodes)),
        #     num_txn_workers=[1, 2, 4, 8, 10, 12, 16, 20],
        #     num_txns=[500_000],

        #     tpcc_new_order_remote_prob=ComputedParam(remote_prob),
        #     tpcc_payment_remote_prob=ComputedParam(
        #         lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
        #     use_switch=[False, True]
        # )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::COUNTER',
        'ORDER_CNT_MAX': '10'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)

    # setup(servers, firmware='p4db_tpcc', override={
    #     'CC_SCHEME': 'CC_Scheme::WAIT_DIE',
    #     'SWITCH_NO_CONFLICT': 'true',
    #     'LM_ON_SWITCH': 'false',
    #     'ENABLED_STATS': 'StatsBitmask::COUNTER',
    #     'ORDER_CNT_MAX': '10'
    # })
    # reg_exp(servers=servers, raise_on_rc=False,
    #         max_restarts=0, params=exp())(p4db)


def tpcc_cycles_old(servers):
    cycles_csv_id = counter()

    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_tpcc_cycles.csv',
            num_nodes=[8],
            workload=['tpcc'],

            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4], num_nodes)),
            num_txn_workers=[20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[
                10, 20, 30, 40, 50, 60, 70, 80, 90],  # [20],
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[False, True],
            csv_file_cycles=ComputedParam(
                lambda _id: ['../tpcc_cycles_{_id}.csv'.format(_id=next(cycles_csv_id))])
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::COUNTER | StatsBitmask::CYCLES',
        'ORDER_CNT_MAX': '10'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


def tpcc_cycles(servers):
    def exp():
        return ParameterGrid(
            _id=next(expid),
            _run=range(RUNS),
            csv_file='exp_tpcc_cycles_detail.csv',
            num_nodes=[8],
            workload=['tpcc'],

            tpcc_num_warehouses=ComputedParam(
                lambda num_nodes: scale([1, 2, 4, 8, 16, 32], num_nodes)),
            num_txn_workers=[20],
            num_txns=[500_000],

            tpcc_new_order_remote_prob=[
                20],
            # 10, 20, 30, 40, 50, 60, 70, 80, 90],
            tpcc_payment_remote_prob=ComputedParam(
                lambda tpcc_new_order_remote_prob: tpcc_new_order_remote_prob),
            use_switch=[True, False]
        )

    setup(servers, firmware='p4db_tpcc', override={
        'CC_SCHEME': 'CC_Scheme::NO_WAIT',
        'SWITCH_NO_CONFLICT': 'true',
        'LM_ON_SWITCH': 'false',
        'ENABLED_STATS': 'StatsBitmask::CYCLES',
        'ORDER_CNT_MAX': '10'
    })
    reg_exp(servers=servers, raise_on_rc=False,
            max_restarts=0, params=exp())(p4db)


# YCSB
# ycsb_a(servers[lambda s: s.id.startswith('node')])
# ycsb_b(servers[lambda s: s.id.startswith('node')])
# ycsb_c(servers[lambda s: s.id.startswith('node')])
# ycsb_d(servers[lambda s: s.id.startswith('node')])
# ycsb_skew(servers[lambda s: s.id.startswith('node')])
# ycsb_skew2(servers[lambda s: s.id.startswith('node')])
# ycsb_cycles(servers[lambda s: s.id.startswith('node')])
# ycsb_optis(servers[lambda s: s.id.startswith('node')])


# SmallBank
# smallbank_e(servers[lambda s: s.id.startswith('node')])
# smallbank_f(servers[lambda s: s.id.startswith('node')])
# smallbank_g(servers[lambda s: s.id.startswith('node')])
# smallbank_h(servers[lambda s: s.id.startswith('node')])
# smallbank_skew(servers[lambda s: s.id.startswith('node')])
# smallbank_skew2(servers[lambda s: s.id.startswith('node')])


# TPC-C
# tpcc_i(servers[lambda s: s.id.startswith('node')])
# tpcc_j(servers[lambda s: s.id.startswith('node')])
# tpcc_k(servers[lambda s: s.id.startswith('node')])
# tpcc_l(servers[lambda s: s.id.startswith('node')])


# Partitioning Comparison
# part_comp(servers[lambda s: s.id.startswith('node')])
# Author feedback: not all hot set fits to switch
# ycsb_hotcold(servers[lambda s: s.id.startswith('node')])
# smallbank_hotcold(servers[lambda s: s.id.startswith('node')])
# tpcc_hotsize(servers[lambda s: s.id.startswith('node')])
# tpcc_chiller(servers[lambda s: s.id.startswith('node')])
# ycsb_overhot(servers[lambda s: s.id.startswith('node')])
# tpcc_aborts(servers[lambda s: s.id.startswith('node')])
# tpcc_cycles(servers[lambda s: s.id.startswith('node')])
