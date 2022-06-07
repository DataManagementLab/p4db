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
        SKIP = 0x00,
        PAYMENT = 0x01,
        NEW_ORDER = 0x02,
        ${no_stock_instr_types}
        STOP = 0x80,
        NEG_STOP = 0x7f
    }

    header no_stock_t {
        InstrType_t type;   // set to SKIP after processing
        bit<16> s_id;
        bit<32> ol_quantity;
        bit<8> is_remote;
    }

    header payment_t {
        InstrType_t type;   // set to SKIP after processing
        bit<16> w_id;
        bit<16> d_id;
        bit<32> h_amount;
    }

    header new_order_t {
        InstrType_t type;   // set to SKIP after processing
        bit<16> d_id;
        bit<32> d_next_o_id;
    }


    header next_type_t {
        InstrType_t type;
    }


    struct header_t {
        ethernet_t ethernet;
        msg_t msg;
        info_t info;

        ${no_stock_hdrs}
        payment_t payment;
        new_order_t new_order;
        next_type_t next_type;
    }

    struct metadata_t {}
    '''

    @property
    def no_stock_instr_types(self):
        for i in range(self.NUM_REGS):
            yield f'NO_STOCK_{i} = 0x{i+3:02x},'

    @property
    def no_stock_hdrs(self):
        yield f'no_stock_t[{self.NUM_INSTR-1}] no_stock_skip;       // Worst case we need to skip {self.NUM_INSTR-1} instructions'

        for i in range(self.NUM_REGS):
            yield f'no_stock_t no_stock_{i};'


class Parser(Snippet):
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
                default: parse_skips;
            }
        }

        state set_high_prio {
            parser_prio.set(7);
            transition parse_skips;
        }

        state parse_skips {
            transition select(pkt.lookahead<InstrType_t>()) {
                InstrType_t.SKIP: parse_no_stock_skip;
                default: parse_instr;
            }
        }

        state parse_no_stock_skip {
            pkt.extract(hdr.no_stock_skip.next);
            transition parse_skips;
        }

        ${state_parse_instr}

        state parse_next_type {
            pkt.extract(hdr.next_type);
            transition accept;
        }

        ${state_parse_no_stock_x}

        state parse_new_order {
            pkt.extract(hdr.new_order);
            transition parse_instr;
        }

        state parse_payment {
            pkt.extract(hdr.payment);
            transition parse_instr;
        }
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
        selections = C_for(
            'InstrType_t.NO_STOCK_${i}: parse_no_stock_${i};', range(self.NUM_REGS))

        return C('''
        state parse_instr {
            transition select(pkt.lookahead<InstrType_t>()) {
                InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT
                ${selections}
                InstrType_t.PAYMENT: parse_payment;
                InstrType_t.NEW_ORDER: parse_new_order;
                default: reject;
            }
        }
        ''', selections=selections)

    @property
    def state_parse_no_stock_x(self):
        return C_for('''
        state parse_no_stock_${i} {
            pkt.extract(hdr.no_stock_${i});
            transition parse_instr;
        }
        ''', range(self.NUM_REGS))


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
            ig_tm_md.bypass_egress = 1;
            if (hdr.info.isValid() && !hdr.payment.isValid()) {

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

            ${reg_lockfree_instr}

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

                hdr.info.recircs = hdr.info.recircs + 1;
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
        yield Register(
            type='bit<32>',
            idx_type='bit<16>',
            size=self.REG_SIZE,
            default_val=f'0x00000000',
            name=f'reg_payment_w_ytd'
        )
        yield RegisterAction(
            in_type='bit<32>',
            idx_type='bit<16>',
            out_type='bit<32>',
            reg_name=f'reg_payment_w_ytd',
            name=f'reg_payment_w_ytd_access',
            body=C('''\
                rv = value;
                value = value + hdr.payment.h_amount;
            ''')
        )
        yield Register(
            type='bit<32>',
            idx_type='bit<16>',
            size=self.REG_SIZE,
            default_val=f'0x00000000',
            name=f'reg_payment_d_ytd'
        )
        yield RegisterAction(
            in_type='bit<32>',
            idx_type='bit<16>',
            out_type='bit<32>',
            reg_name=f'reg_payment_d_ytd',
            name=f'reg_payment_d_ytd_access',
            body=C('''\
                rv = value;
                value = value + hdr.payment.h_amount;
            ''')
        )
        yield Register(
            type='bit<32>',
            idx_type='bit<16>',
            size=self.REG_SIZE,
            default_val=f'0x00000000',
            name=f'reg_no_d_next_o_id'
        )
        yield RegisterAction(
            in_type='bit<32>',
            idx_type='bit<16>',
            out_type='bit<32>',
            reg_name=f'reg_no_d_next_o_id',
            name=f'reg_no_d_next_o_id_access',
            body=C('''\
                rv = value;
                value = value + 1;
            ''')
        )

        for i in range(self.NUM_REGS):
            yield Register(
                type='bit<32>',
                idx_type='bit<16>',
                size=self.REG_SIZE,
                default_val=f'0x00000000',
                name=f'reg_s_ytd_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_s_ytd_{i}',
                name=f'reg_s_ytd_{i}_access',
                body=C('''\
                    rv = value;
                    value = value + hdr.no_stock_${i}.ol_quantity;
                ''', i=i)
            )
            yield Register(
                type='bit<32>',
                idx_type='bit<16>',
                size=self.REG_SIZE,
                default_val=f'0x00000000',
                name=f'reg_s_quantity_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_s_quantity_{i}',
                name=f'reg_s_quantity_{i}_access',
                body=C('''\
                    rv = value;
                    
                    if (value < hdr.no_stock_${i}.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                        value = value + 91;
                    }
                    value = value - hdr.no_stock_${i}.ol_quantity;
                ''', i=i)
            )
            yield Register(
                type='bit<32>',
                idx_type='bit<16>',
                size=self.REG_SIZE,
                default_val=f'0x00000000',
                name=f'reg_s_order_cnt_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_s_order_cnt_{i}',
                name=f'reg_s_order_cnt_{i}_access',
                body=C('''\
                    rv = value;
                    value = value + 1;
                ''', i=i)
            )
            yield Register(
                type='bit<32>',
                idx_type='bit<16>',
                size=self.REG_SIZE,
                default_val=f'0x00000000',
                name=f'reg_s_remote_cnt_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_s_remote_cnt_{i}',
                name=f'reg_s_remote_cnt_{i}_access',
                body=C('''\
                    rv = value;
                    value = value + (bit<32>) hdr.no_stock_${i}.is_remote;
                ''', i=i)
            )

    @property
    def reg_lockfree_instr(self):
        yield '// lockfree'
        yield '''\
            else if (hdr.payment.isValid()) {
                reg_payment_w_ytd_access.execute(hdr.payment.w_id);
                reg_payment_d_ytd_access.execute(hdr.payment.d_id);
                hdr.payment.type = InstrType_t.SKIP;
                reply();
            }
        '''
        yield '''\
            // we can move this here because new-order header is always at the end and only valid if all previous stocks are parsed
            // for which a lock or no lock is needed (transitivity)
            if (hdr.new_order.isValid()) {
                hdr.new_order.d_next_o_id = reg_no_d_next_o_id_access.execute(hdr.new_order.d_id);
                hdr.new_order.type = InstrType_t.SKIP;
            }
        '''

    @property
    def reg_instrs(self):
        for i in range(self.NUM_REGS):
            yield C('''\
                if (hdr.no_stock_${i}.isValid()) {
                    reg_s_ytd_${i}_access.execute(hdr.no_stock_${i}.s_id);
                    reg_s_quantity_${i}_access.execute(hdr.no_stock_${i}.s_id);
                    reg_s_order_cnt_${i}_access.execute(hdr.no_stock_${i}.s_id);
                    reg_s_remote_cnt_${i}_access.execute(hdr.no_stock_${i}.s_id);
                    hdr.no_stock_${i}.type = InstrType_t.SKIP;
                }
            ''', i=i)


if __name__ == '__main__':
    NUM_INSTR = 15      # --> how many the parser should skip
    NUM_REGS = 10
    REG_SIZE = int(65536/2 + 32768/4)

    code = P4Program(
        headers=Headers(NUM_REGS=NUM_REGS, NUM_INSTR=NUM_INSTR),
        parser=Parser(NUM_REGS=NUM_REGS),
        utils=Utils(),
        ingress=Ingress(NUM_REGS=NUM_REGS, REG_SIZE=REG_SIZE)
    )

    if len(sys.argv) < 2:
        filename = '/dev/stdout'
        output = sys.stdout
    else:
        filename = sys.argv[1]
        output = open(filename, 'w')

    loc = write(output, code)
    print(f'{filename:<16}: {loc} LOC generated', file=sys.stderr)
