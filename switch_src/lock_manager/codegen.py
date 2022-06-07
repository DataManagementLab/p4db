import sys
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

    enum bit<8> AccessMode_t {
        INVALID = 0x00,
        READ = 0x01,
        WRITE = 0x02
    }


    header tuple_msg_t {
        bit<64> ts;
        bit<64> tid;
        bit<64> rid;
        AccessMode_t mode;
        bit<8> by_switch;
        bit<16> lock_idx;
        // tuple_data is optional payload
    }


    header tuple_t {
        ${data_tuples}
    }


    struct header_t {
        ethernet_t ethernet;
        msg_t msg;
        tuple_msg_t tuple_get_req;
        tuple_msg_t tuple_put_req;
        tuple_t data;
    }

    struct metadata_t {}
    '''

    @property
    def data_tuples(self):
        for i in range(self.NUM_REGS):
            yield f'bit<32> field_{i};'


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
                MsgType_t.TUPLE_GET_REQ: parse_tuple_get_req;
                MsgType_t.TUPLE_PUT_REQ: parse_tuple_put_req;
                default: accept;
            }
        }

        state parse_tuple_get_req {
            pkt.extract(hdr.tuple_get_req);
            transition accept;
        }

        state parse_tuple_put_req {
            pkt.extract(hdr.tuple_put_req);
            transition select(hdr.tuple_put_req.mode) {
                AccessMode_t.WRITE: parse_data;
                default: accept;
            }
        }

        state parse_data {
            pkt.extract(hdr.data);
            transition accept;
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

        ${recirculation_actions}

        ${locking}

        ${data_regs}


        apply {
            if (hdr.tuple_get_req.isValid() && hdr.tuple_get_req.by_switch == 0xaa) {
                AccessMode_t mode = hdr.tuple_get_req.mode;
                hdr.tuple_get_req.by_switch = hdr.tuple_get_req.by_switch + 0xaa;
                bit<16> idx = hdr.tuple_get_req.lock_idx;
                bit<8> granted;
                if (mode == AccessMode_t.READ) {
                    granted = try_lock_shared.execute(idx);
                } else if (mode == AccessMode_t.WRITE) {
                    granted = try_lock_exclusive.execute(idx);
                }

                hdr.msg.type = MsgType_t.TUPLE_GET_RES;
                if (granted == 1) {
                    hdr.data.setValid();
                    ${reads}
                } else {
                    hdr.tuple_get_req.mode = AccessMode_t.INVALID;
                }
                reply();
            } else if (hdr.tuple_put_req.isValid() && hdr.tuple_put_req.by_switch == 0x54) {    // 0xaa + 0xaa = 0x54
                bit<16> idx = hdr.tuple_put_req.lock_idx;
                unlock.execute(idx);
                if (hdr.data.isValid()) {
                    ${writes}
                    hdr.data.setInvalid();
                }
                hdr.msg.type = MsgType_t.TUPLE_PUT_RES;

                reply();
            }

            l2fwd.apply();
            ig_tm_md.bypass_egress = 1;
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
                     type='bit<32>',
                     idx_type='bit<16>',
                     size=REG_SIZE,
                     default_val='0',
                     name='locks'
                 ),
                 try_lock=RegisterAction(
                     in_type='bit<32>',
                     idx_type='bit<16>',
                     out_type='bit<8>',
                     reg_name='locks',
                     name='try_lock_exclusive',
                     body=C('''\
                    if (value == 0) {
                        value = 0x7fffffff;
                        rv = 1;
                    } else {
                        rv = 0;
                    }
                ''')
                 ),
                 unlock=RegisterAction(
                     in_type='bit<32>',
                     idx_type='bit<16>',
                     out_type='bit<8>',
                     reg_name='locks',
                     name='try_lock_shared',
                     body=C('''\
                    if (value != 0x7fffffff) {
                        value = value + 1;
                        rv = 1;
                    } else {
                        rv = 0;
                    }
                ''')
                 ),
                 is_locked=RegisterAction(
                     in_type='bit<32>',
                     idx_type='bit<16>',
                     out_type='bit<8>',
                     reg_name='locks',
                     name='unlock',
                     body=C('''\
                    if (value == 0x7fffffff) {
                        value = 0;
                    } else {
                        value = value - 1;
                    }
                    rv = 1;
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
                default_val=f'0x00000000',
                name=f'data_{i}'
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'data_{i}',
                name=f'read_{i}',
                body=C('''\
                    rv = value;
                ''', i=i)
            )
            yield RegisterAction(
                in_type='bit<32>',
                idx_type='bit<16>',
                out_type='bit<32>',
                reg_name=f'data_{i}',
                name=f'write_{i}',
                body=C('''\
                    value = hdr.data.field_${i};
                ''', i=i)
            )

    @property
    def reads(self):
        for i in range(self.NUM_REGS):
            yield f'hdr.data.field_{i} = read_{i}.execute(idx);'

    @property
    def writes(self):
        for i in range(self.NUM_REGS):
            yield f'write_{i}.execute(idx);'


if __name__ == '__main__':
    NUM_REGS = 3
    # REG_SIZE = int(65536/8)
    REG_SIZE = int(1024*16+1)

    code = P4Program(
        headers=Headers(NUM_REGS=NUM_REGS),
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
