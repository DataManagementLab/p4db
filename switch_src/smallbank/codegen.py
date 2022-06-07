import sys
import string
import itertools


from codegen import *


class Headers(Snippet):
    '''\
    typedef bit<48> mac_addr_t;
    typedef bit<16> ether_type_t;

    header ethernet_t {
        mac_addr_t dst_addr;
        mac_addr_t src_addr;
        ether_type_t ether_type;
    }

    enum bit<32> MsgType_t {
        // Handled by System
        INIT = 0x01000100,
        BARRIER = 0x02000100,

        TUPLE_GET_REQ  = 0x01000000,
        TUPLE_GET_RES = 0x02000000,
        TUPLE_PUT_REQ  = 0x03000000,
        TUPLE_PUT_RES = 0x04000000,

        SWITCH_TXN = 0x05000000
    }

    typedef bit<32> node_t;
    typedef bit<64> msgid_t;

    header msg_t {
        bit<16> padding;    // 14 bytes for eth_hdr_t, add 2 bytes
        MsgType_t type;
        node_t sender;
        msgid_t msg_id;
    }

    struct lock_pair {
        bit<8> left;
        bit<8> right;
    }

    header info_t {
        bit<1> has_lock;
        bit<6> empty;
        bit<1> multipass;   // only last bit written for bool
        bit<32> recircs;
        lock_pair locks;    // TODO maybe move to empty slot with 1 bit each
    }

    enum bit<8> InstrType_t {
        ${instr_types}
        SKIP = 0b01000000,
        STOP = 0b10000000,
        NEG_STOP = 0b01111111
    }

    header balance_t {
        InstrType_t type;   // set to SKIP after processing
        bit<16> c_id;
        bit<32> saving_bal;
        bit<32> checking_bal;
    }

    header deposit_checking_t {
        InstrType_t type;   // set to SKIP after processing
        bit<16> c_id;
        bit<32> checking_bal;
    }

    header transact_saving_t {
        InstrType_t type;   // set to SKIP after processing
        bit<16> c_id;
        bit<32> saving_bal;
    }

    header amalgamate_t {
        InstrType_t type;   // DON'T set to SKIP after processing
        bit<16> c_id_0;
        bit<32> saving_bal;
        bit<32> checking_bal;
    }



    header next_type_t {
        InstrType_t type;
    }


    struct header_t {
        ethernet_t ethernet;
        msg_t msg;
        info_t info;
        ${instr_hdrs}

        next_type_t next_type;
    }

    struct metadata_t {}
    '''

    @property
    def instr_types(self):
        for i in range(self.NUM_REGS):
            yield f'BALANCE_{i} = 0x{0b00000000+i:02x},'
            yield f'DEPOSIT_CHECKING_{i} = 0x{0b00010000+i:02x},'
            yield f'TRANSACT_SAVING_{i} = 0x{0b00100000+i:02x},'
            yield f'AMALGAMATE_{i} = 0x{0b00110000+i:02x},'
            # yield f'SEND_PAYMENT_{i} = 0x{next(val):02x},'
            # yield f'WRITE_CHECK_{i} = 0x{next(val):02x},'
        yield 'ABORT = 0x80,'

    @property
    def instr_hdrs(self):
        yield f'deposit_checking_t deposit_checking_skip;'
        for i in range(self.NUM_REGS):
            yield f'balance_t balance_{i};'
            yield f'deposit_checking_t deposit_checking_{i};'
            yield f'transact_saving_t transact_saving_{i};'
            yield f'amalgamate_t amalgamate_{i};'


class IngressParser(Snippet):
    '''\
    parser IngressParser(
            packet_in pkt,
            out header_t hdr,
            out metadata_t ig_md,
            out ingress_intrinsic_metadata_t ig_intr_md) {

        TofinoIngressParser() tofino_parser;
        ParserPriority() parser_prio;

        state start {
            tofino_parser.apply(pkt, ig_intr_md);
            transition parse_ethernet;
        }

        state parse_ethernet {
            pkt.extract(hdr.ethernet);
            transition select(hdr.ethernet.ether_type) {
                0x1000: parse_msg;
                default: accept;
            }
        }

        state parse_msg {
            pkt.extract(hdr.msg);
            transition select(hdr.msg.type) {
                MsgType_t.SWITCH_TXN : parse_info;
                default: accept;
            }
        }

        state parse_info {
            pkt.extract(hdr.info);
            transition select(hdr.info.has_lock) {
                1: set_high_prio;
                default: parse_instr;
            }
        }

        state set_high_prio {
            parser_prio.set(7);
            transition parse_instr;
        }

        ${state_parse_instr}

        state parse_next_type {
            pkt.extract(hdr.next_type);
            transition accept;
        }

        ${state_parse_hdrs}
    }

    control IngressDeparser(
            packet_out pkt,
            inout header_t hdr,
            in metadata_t ig_md,
            in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

        apply {
            pkt.emit(hdr);
        }
    }
    '''

    @property
    def state_parse_instr(self):
        yield 'state parse_instr {'
        yield '    transition select(pkt.lookahead<InstrType_t>()) {'
        yield '        InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT'
        yield '        InstrType_t.SKIP &&& InstrType_t.SKIP: parse_deposit_checking_skip;  // if SKIP_BIT is set'

        for i in range(self.NUM_REGS):
            yield f'InstrType_t.BALANCE_{i}: parse_balance_{i};'
            yield f'InstrType_t.DEPOSIT_CHECKING_{i}: parse_deposit_checking_{i};'
            yield f'InstrType_t.TRANSACT_SAVING_{i}: parse_transact_saving_{i};'
            yield f'InstrType_t.AMALGAMATE_{i}: parse_amalgamate_{i};'
        yield '        default: reject;'
        yield '    }'
        yield '}'

    @property
    def state_parse_hdrs(self):
        def state(name):
            return C('''\
                state parse_${name} {
                    pkt.extract(hdr.${name});
                    transition parse_instr;
                }
            ''', name=name)

        yield state(f'deposit_checking_skip')

        for i in range(self.NUM_REGS):
            yield state(f'balance_{i}')
            yield state(f'deposit_checking_{i}')
            yield state(f'transact_saving_{i}')
            yield state(f'amalgamate_{i}')


class Ingress(Snippet):
    '''\
    control Ingress(
            inout header_t hdr,
            inout metadata_t ig_md,
            in ingress_intrinsic_metadata_t ig_intr_md,
            in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
            inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
            inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {


        ${l2fwd}

        ${locking}

        ${recirculation_actions}

        ${data_regs}


        bool do_recirc = false;
        bool do_access = false;

        apply {
            // ig_tm_md.bypass_egress = 1;
            if (hdr.info.isValid()) {

                if (hdr.info.multipass == 0) {      // no lock needed, one-go
                    if (is_locked.execute(0) == 1) {
                        recirculate(68);
                        do_recirc = true;
                    } else {
                        do_access = true;
                        reply();
                    }
                } else if (hdr.info.has_lock == 0) {     // first one we lock
                    if (try_lock.execute(0) == 0) {
                        recirculate(68);
                        do_recirc = true;
                    } else {
                        hdr.info.has_lock = 1;
                        do_access = true;

                        hdr.next_type.type = (InstrType_t) (hdr.next_type.type & InstrType_t.NEG_STOP);     // remove stop bit
                        do_recirc = true;
                        recirculate_fast();
                    }
                } else if (hdr.next_type.type == InstrType_t.STOP) {    // last one we unlock
                    unlock.execute(0);
                    hdr.info.has_lock = 0;
                    do_access = true;
                    reply();
                } else {                                // we still have lock and keep it
                    do_access = true;
                    hdr.next_type.type = (InstrType_t) (hdr.next_type.type & InstrType_t.NEG_STOP);     // remove stop bit
                    do_recirc = true;
                    recirculate_fast();
                }
            }

            if (do_access) {
                ${reg_instrs}
            }

            if (!do_recirc) {
                l2fwd.apply();  // send digest if no hit
            }        
        }
    }
    '''

    @property
    def l2fwd(self):
        return '''\
        action reply() {
            mac_addr_t old_mac = hdr.ethernet.dst_addr;
            hdr.ethernet.dst_addr = hdr.ethernet.src_addr;
            hdr.ethernet.src_addr = old_mac;
        }

        action send(PortId_t port) {
            ig_tm_md.ucast_egress_port = port;
        }

        action drop() {
            ig_dprsr_md.drop_ctl = 1;
        }


        table l2fwd {
            key = {
                hdr.ethernet.dst_addr: exact;
            }
            actions = {
                send;
                @defaultonly drop();
            }
            const default_action = drop;
            size = 32;
        }
        '''

    @property
    def recirculation_actions(self):
        return '''
            action recirculate(PortId_t port) {
                // enable loopback: https://forum.barefootnetworks.com/hc/en-us/community/posts/360038446114-packet-recirculate-to-a-port-that-is-down
                ig_tm_md.ucast_egress_port = ig_intr_md.ingress_port[8:7] ++ port[6:0];
                // ig_tm_md.bypass_egress = 1;

                // hdr.info.recircs = hdr.info.recircs + 1;
            }

            action recirculate_fast() {
                ig_tm_md.ucast_egress_port = 156;
                // ig_tm_md.bypass_egress = 1;

                hdr.info.recircs = hdr.info.recircs + 1;
            }
        '''

    @property
    def locking(self):
        return C('''\
            ${lock_reg}
            ${try_lock}
            ${unlock}
            ${is_locked}
        ''',
                 lock_reg=Register(
                     type='lock_pair',
                     idx_type='bit<1>',
                     size=1,
                     default_val='{0, 0}',
                     name='switch_lock'
                 ),
                 try_lock=RegisterAction(
                     in_type='lock_pair',
                     idx_type='bit<1>',
                     out_type='bit<1>',
                     reg_name='switch_lock',
                     name='try_lock',
                     body=C('''\
                    if ((hdr.info.locks.left + value.left) == 2) {
                        rv = 0;
                    } else if ((hdr.info.locks.right + value.right) == 2) {
                        rv = 0;
                    } else {
                        rv = 1;
                        value.left  = value.left + hdr.info.locks.left;
                        value.right  = value.right + hdr.info.locks.right;
                    }
                ''')
                 ),
                 unlock=RegisterAction(
                     in_type='lock_pair',
                     idx_type='bit<1>',
                     out_type='bit<1>',
                     reg_name='switch_lock',
                     name='unlock',
                     body=C('''\
                    value.left = value.left - hdr.info.locks.left;
                    value.right = value.right - hdr.info.locks.right;
                ''')
                 ),
                 is_locked=RegisterAction(
                     in_type='lock_pair',
                     idx_type='bit<1>',
                     out_type='bit<1>',
                     reg_name='switch_lock',
                     name='is_locked',
                     body=C('''\
                    if (value.left > 0 || value.right > 0) {
                        rv = 1;
                    } else {
                        rv = 0;
                    }
                ''')
                 )
                 )

    @property
    def data_regs(self):
        for i in range(self.NUM_REGS):
            yield Register(
                type='bit<32>',
                idx_type='bit<16>',
                size=self.REG_SIZE,
                default_val=f'0x00000010',
                name=f'reg_saving_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_saving_{i}',
                name=f'reg_saving_{i}_read',
                body=C('''\
                    rv = value;
                ''', i=i)
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_saving_{i}',
                name=f'reg_saving_{i}_zero',
                body=C('''\
                    rv = value;
                    value = 0;
                ''', i=i)
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_saving_{i}',
                name=f'reg_saving_{i}_transact',
                body=C('''\
                    int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
                    int<32> b = (int<32>) hdr.transact_saving_${i}.saving_bal;

                    if (a + b >= 0) {
                        value = value + hdr.transact_saving_${i}.saving_bal;
                        // rv = value;
                    } else {
                        rv = 0xffffffff;    // signal failure
                    }
                ''', i=i)
            )

            yield Register(
                type='bit<32>',
                idx_type='bit<16>',
                size=self.REG_SIZE,
                default_val=f'0x00000020',
                name=f'reg_checking_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_checking_{i}',
                name=f'reg_checking_{i}_read',
                body=C('''\
                    rv = value;
                ''', i=i)
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_checking_{i}',
                name=f'reg_checking_{i}_zero',
                body=C('''\
                    rv = value;
                    value = 0;
                ''', i=i)
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_checking_{i}',
                name=f'reg_checking_{i}_deposit',
                body=C('''\
                value = value + hdr.deposit_checking_${i}.checking_bal;
                    rv = value;
                ''', i=i)
            )

    @property
    def reg_instrs(self):
        for i in range(self.NUM_REGS):
            yield C('''\
                if (hdr.balance_${i}.isValid()) {
                    hdr.balance_${i}.saving_bal = reg_saving_${i}_read.execute(hdr.balance_${i}.c_id);
                    hdr.balance_${i}.checking_bal = reg_checking_${i}_read.execute(hdr.balance_${i}.c_id);
                    // hdr.balance_${i}.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
                }
                else if (hdr.deposit_checking_${i}.isValid()) {
                    hdr.deposit_checking_${i}.checking_bal = reg_checking_${i}_deposit.execute(hdr.deposit_checking_${i}.c_id);
                    hdr.deposit_checking_${i}.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
                }
                else if (hdr.transact_saving_${i}.isValid()) {
                    hdr.transact_saving_${i}.saving_bal = reg_saving_${i}_transact.execute(hdr.transact_saving_${i}.c_id);
                    hdr.transact_saving_${i}.type = InstrType_t.SKIP;
                }
                else if (hdr.amalgamate_${i}.isValid()) {
                    hdr.amalgamate_${i}.saving_bal = reg_saving_${i}_zero.execute(hdr.amalgamate_${i}.c_id_0);
                    hdr.amalgamate_${i}.checking_bal = reg_checking_${i}_zero.execute(hdr.amalgamate_${i}.c_id_0);
                    // hdr.transact_saving_${i}.type = InstrType_t.SKIP;
                }
            ''', i=i)


class EgressHeaders(Snippet):
    '''\
    header abort_t {
        InstrType_t type;
    }

    header write_check_egress_t {      // for egress
        InstrType_t type;   // header will be deleted
        bit<16> c_id;
        bit<32> balance;
    }

    header amalgamate_egress_t {
        InstrType_t type;   // header will be deleted
        InstrType_t c_type;
        bit<16> c_id_1;
    }

    header send_payment_egress_t {
        InstrType_t type;   // header will be deleted
        InstrType_t c_type_0;
        InstrType_t c_type_1;
        bit<16> c_id_1;
        bit<32> balance;
    }


    struct egress_header_t {
        ethernet_t ethernet;
        msg_t msg;
        info_t info;

        abort_t abort;

        balance_t balance;
        write_check_egress_t write_check;
        deposit_checking_t deposit_checking;    // different types
        deposit_checking_t deposit_checking_2;    // For payment

        amalgamate_t amalgamate;
        amalgamate_egress_t amalgamate_egress;

        send_payment_egress_t send_payment;

        next_type_t next_type;  // Fix this for ending?
    }

    struct egress_metadata_t {}
    '''


class EgressParser(Snippet):
    '''\
    parser EgressParser(
            packet_in pkt,
            out egress_header_t hdr,
            out egress_metadata_t eg_md,
            out egress_intrinsic_metadata_t eg_intr_md) {

        TofinoEgressParser() tofino_parser;

        state start {
            tofino_parser.apply(pkt, eg_intr_md);
            transition parse_ethernet;
        }

        state parse_ethernet {
            pkt.extract(hdr.ethernet);
            transition select(hdr.ethernet.ether_type) {
                0x1000: parse_msg;
                default: accept;
            }
        }

        state parse_msg {
            pkt.extract(hdr.msg);
            transition select(hdr.msg.type) {
                MsgType_t.SWITCH_TXN : parse_info;
                default: accept;
            }
        }

        state parse_info {
            pkt.extract(hdr.info);

            transition select(hdr.info.has_lock) {
                (1w1) : check_recircs;
                default: accept;
            }
        }

        state check_recircs {
            transition select(hdr.info.recircs) {
                0x00000001 : parse_instr;
                default: accept;
            }
        }

        state parse_instr {
            transition select(pkt.lookahead<InstrType_t>()) {
                InstrType_t.BALANCE_0 &&& 0b00110000: parse_write_check;
                InstrType_t.AMALGAMATE_0 &&& 0b00110000: parse_amalgamate;
                InstrType_t.DEPOSIT_CHECKING_0 &&& 0b00110000: parse_send_payment;
                default: accept; // parse_send_payment;
            }
        }

        state parse_write_check {
            pkt.extract(hdr.balance);
            pkt.extract(hdr.write_check);
            transition accept;
        }

        state parse_amalgamate {
            pkt.extract(hdr.amalgamate);
            pkt.extract(hdr.amalgamate_egress);
            transition accept;
        }


        state parse_send_payment {
            pkt.extract(hdr.deposit_checking);
            pkt.extract(hdr.send_payment);
            transition accept;
        }
    }

    control EgressDeparser(
            packet_out pkt,
            inout egress_header_t hdr,
            in egress_metadata_t eg_md,
            in egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md) {

        apply {
            pkt.emit(hdr);
        }
    }
    '''


class Egress(Snippet):
    '''\
    control Egress(
            inout egress_header_t hdr,
            inout egress_metadata_t eg_md,
            in egress_intrinsic_metadata_t eg_intr_md,
            in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
            inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
            inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {

        int<32> var_0;

        action sum_balance() {
            var_0 = (int<32>) hdr.balance.saving_bal + (int<32>) hdr.balance.checking_bal;
        }

        action sub_write_check_bal() {
            var_0 = var_0 - (int<32>) hdr.write_check.balance;
        }

        action sum_amalgamate() {
            var_0 = (int<32>) hdr.amalgamate.saving_bal + (int<32>) hdr.amalgamate.checking_bal;
        }

        action sub_payment() {
            var_0 = (int<32>) hdr.deposit_checking.checking_bal - (int<32>) hdr.send_payment.balance;
        }

        apply {
            // read balance rewrite write_check as

            if (hdr.write_check.isValid()) {
                sum_balance();
                sub_write_check_bal();

                int<32> deposit_bal;
                if (var_0 < 0) {
                    deposit_bal = (int<32>) hdr.write_check.balance + 1;      // overdraw
                } else {
                    deposit_bal = (int<32>) hdr.write_check.balance;
                }

                bit<8> type = 4w0b01 ++ hdr.balance.type[3:0];
                bit<16> c_id = hdr.write_check.c_id;
                hdr.balance.setInvalid();
                hdr.write_check.setInvalid();

                hdr.deposit_checking.setValid();
                hdr.deposit_checking.type = (InstrType_t) type;   // use same partition as balance
                hdr.deposit_checking.c_id = c_id;
                hdr.deposit_checking.checking_bal = (bit<32>) -deposit_bal;
            }
            else if (hdr.amalgamate_egress.isValid()) {
                sum_amalgamate();

                InstrType_t c_type = hdr.amalgamate_egress.c_type;
                bit<16> c_id_1 = hdr.amalgamate_egress.c_id_1;
                hdr.amalgamate.setInvalid();
                hdr.amalgamate_egress.setInvalid();

                hdr.deposit_checking.setValid();
                hdr.deposit_checking.type = c_type;
                hdr.deposit_checking.c_id = c_id_1;
                hdr.deposit_checking.checking_bal = (bit<32>) var_0;
            }
            else if (hdr.send_payment.isValid()) {
                sub_payment();

                if (var_0 < 0) {
                    // abort
                    hdr.abort.setValid();
                    hdr.abort.type = InstrType_t.ABORT;
                } else {
                    InstrType_t c_type_0 = hdr.send_payment.c_type_0;
                    InstrType_t c_type_2 = hdr.send_payment.c_type_1;
                    bit<16> c_id_1 = hdr.send_payment.c_id_1;
                    int<32> balance = (int<32>) hdr.send_payment.balance;
                    hdr.send_payment.setInvalid();


                    hdr.deposit_checking.type = c_type_0;
                    // c_id stays the same
                    hdr.deposit_checking.checking_bal = (bit<32>) -balance;  // remove from c_01

                    hdr.deposit_checking_2.setValid();
                    hdr.deposit_checking_2.type = (InstrType_t) (c_type_2 | InstrType_t.STOP);
                    hdr.deposit_checking_2.c_id = c_id_1;
                    hdr.deposit_checking_2.checking_bal = (bit<32>) balance;
                }
            }
        }
    }
    '''


if __name__ == '__main__':
    NUM_REGS = 10
    REG_SIZE = int(65536/8)

    code = P4Program(
        ingress_headers=Headers(NUM_REGS=NUM_REGS),
        ingress_parser=IngressParser(NUM_REGS=NUM_REGS),
        utils=Utils(),
        ingress=Ingress(NUM_REGS=NUM_REGS, REG_SIZE=REG_SIZE),

        egress_headers=EgressHeaders(),
        egress_parser=EgressParser(),
        egress=Egress(),
    )

    if len(sys.argv) < 2:
        filename = '/dev/stdout'
        output = sys.stdout
    else:
        filename = sys.argv[1]
        output = open(filename, 'w')

    loc = write(output, code)
    print(f'{filename:<16}: {loc} LOC generated', file=sys.stderr)
