import sys


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
        ${reg_instr_types}
        STOP = 0x80,
        NEG_STOP = 0x7f
    }

    enum bit<8> OPCode_t {
        READ = 0x00,
        WRITE = 0x01
    }

    header reg_instr_t {
        InstrType_t type;   // set to SKIP after processing, maybe merge with op
        OPCode_t op;
        bit<16> idx;
        bit<32> data;
    }

    header next_type_t {
        InstrType_t type;
    }


    struct header_t {
        ethernet_t ethernet;
        msg_t msg;
        info_t info;

        ${reg_hdrs}
        next_type_t next_type;
    }

    struct metadata_t {}
    '''

    @property
    def reg_instr_types(self):
        for i in range(self.NUM_REGS):
            yield f'REG_{i} = 0x{i+1:02x},'

    @property
    def reg_hdrs(self):
        yield f'reg_instr_t[{self.NUM_INSTR-1}] reg_skip;       // Worst case we need to skip {self.NUM_INSTR-1} instructions'

        for i in range(self.NUM_REGS):
            yield f'reg_instr_t reg_{i};'


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
                InstrType_t.SKIP: parse_reg_skip;
                default: parse_regs;
            }
        }

        state parse_reg_skip {
            pkt.extract(hdr.reg_skip.next);
            transition parse_skips;
        }

        ${state_parse_regs}

        state parse_next_type {
            pkt.extract(hdr.next_type);
            transition accept;
        }

        ${state_parse_reg_x}
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
    def state_parse_regs(self):
        selections = C_for(
            'InstrType_t.REG_${i}: parse_reg_${i};', range(self.NUM_REGS))

        return C('''
        state parse_regs {
            transition select(pkt.lookahead<InstrType_t>()) {
                InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT
                ${selections}
                default: reject;
            }
        }
        ''', selections=selections)

    @property
    def state_parse_reg_x(self):
        return C_for('''
        state parse_reg_${i} {
            pkt.extract(hdr.reg_${i});
            transition parse_regs;
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
                        recirculate(68);
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
                    recirculate(68);
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

                hdr.info.recircs = hdr.info.recircs + 1;
            }

            action recirculate_fast() {
                ig_tm_md.ucast_egress_port = 156;       // TODO why is method from above not working here?
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
                default_val=f'0x{0x0101*(i+1):04x}',
                name=f'reg_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'reg_{i}',
                name=f'reg_{i}_access',
                body=C('''\
                    rv = value;
                    if (hdr.reg_${i}.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                        value = hdr.reg_${i}.data;
                    }
                ''', i=i)
            )

    @property
    def reg_instrs(self):
        for i in range(self.NUM_REGS):
            yield C('''\
                if (hdr.reg_${i}.isValid()) {
                    hdr.reg_${i}.data = reg_${i}_access.execute(hdr.reg_${i}.idx);
                    hdr.reg_${i}.type = InstrType_t.SKIP;
                }
            ''', i=i)


if __name__ == '__main__':
    NUM_INSTR = 8  # 16
    NUM_REGS = 20  # 40  # 10 / 40
    REG_SIZE = int(65536/2)

    assert(NUM_REGS > 1)
    assert(NUM_INSTR <= NUM_REGS)
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
