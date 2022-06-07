#include <core.p4>

#if __TARGET_TOFINO__ == 2
    #include <t2na.p4>
#else
    #include <tna.p4>
#endif

/************************************************************/
// HEADERS
/************************************************************/
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
    bit<32> field_0;
    bit<32> field_1;
    bit<32> field_2;
}


struct header_t {
    ethernet_t ethernet;
    msg_t msg;
    tuple_msg_t tuple_get_req;
    tuple_msg_t tuple_put_req;
    tuple_t data;
}

struct metadata_t {}


/************************************************************/
// UTILS
/************************************************************/
struct empty_header_t {}
struct empty_metadata_t {}

// Skip egress
control BypassEgress(inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {
    
    action set_bypass_egress() {
        ig_tm_md.bypass_egress = 1w1;
    }
    
    table bypass_egress {
        actions = {
            set_bypass_egress();
        }
        const default_action = set_bypass_egress;
    }
    
    apply {
        bypass_egress.apply();
    }
}


parser TofinoIngressParser(
        packet_in pkt,
        out ingress_intrinsic_metadata_t ig_intr_md) {
    state start {
        pkt.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            1 : parse_resubmit;
            0 : parse_port_metadata;
        }
    }
    
    state parse_resubmit {
        // Parse resubmitted packet here.
        transition reject;
    }
    
    state parse_port_metadata {
        pkt.advance(PORT_METADATA_SIZE);
        transition accept;
    }
}


parser TofinoEgressParser(
        packet_in pkt,
        out egress_intrinsic_metadata_t eg_intr_md) {
    state start {
        pkt.extract(eg_intr_md);
        transition accept;
    }
}


// Empty egress parser/control blocks
parser EmptyEgressParser(
        packet_in pkt,
        out empty_header_t hdr,
        out empty_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {
    state start {
        transition accept;
    }
}

control EmptyEgressDeparser(
        packet_out pkt,
        inout empty_header_t hdr,
        in empty_metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md) {
    apply {}
}

control EmptyEgress(
        inout empty_header_t hdr,
        inout empty_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
    apply {}
}


/************************************************************/
// PARSER
/************************************************************/
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


/************************************************************/
// INGRESS
/************************************************************/
control Ingress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {
    
    
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
    
    
    
    action recirculate(PortId_t port) {
        // enable loopback: https://forum.barefootnetworks.com/hc/en-us/community/posts/360038446114-packet-recirculate-to-a-port-that-is-down
        ig_tm_md.ucast_egress_port = ig_intr_md.ingress_port[8:7] ++ port[6:0];
    }
    
    
    Register<bit<32>, bit<16>>(16385, 0) locks;
    RegisterAction<bit<32>, bit<16>, bit<8>>(locks) try_lock_exclusive = {
        void apply(inout bit<32> value, out bit<8> rv) {
            if (value == 0) {
                value = 0x7fffffff;
                rv = 1;
            } else {
                rv = 0;
            }
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<8>>(locks) try_lock_shared = {
        void apply(inout bit<32> value, out bit<8> rv) {
            if (value != 0x7fffffff) {
                value = value + 1;
                rv = 1;
            } else {
                rv = 0;
            }
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<8>>(locks) unlock = {
        void apply(inout bit<32> value, out bit<8> rv) {
            if (value == 0x7fffffff) {
                value = 0;
            } else {
                value = value - 1;
            }
            rv = 1;
        }
    };
    
    Register<bit<32>, bit<16>>(16385, 0x00000000) data_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(data_0) read_0 = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(data_0) write_0 = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = hdr.data.field_0;
        }
    };
    Register<bit<32>, bit<16>>(16385, 0x00000000) data_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(data_1) read_1 = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(data_1) write_1 = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = hdr.data.field_1;
        }
    };
    Register<bit<32>, bit<16>>(16385, 0x00000000) data_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(data_2) read_2 = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(data_2) write_2 = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = hdr.data.field_2;
        }
    };
    
    
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
                hdr.data.field_0 = read_0.execute(idx);
                hdr.data.field_1 = read_1.execute(idx);
                hdr.data.field_2 = read_2.execute(idx);
            } else {
                hdr.tuple_get_req.mode = AccessMode_t.INVALID;
            }
            reply();
        } else if (hdr.tuple_put_req.isValid() && hdr.tuple_put_req.by_switch == 0x54) {    // 0xaa + 0xaa = 0x54
            bit<16> idx = hdr.tuple_put_req.lock_idx;
            unlock.execute(idx);
            if (hdr.data.isValid()) {
                write_0.execute(idx);
                write_1.execute(idx);
                write_2.execute(idx);
                hdr.data.setInvalid();
            }
            hdr.msg.type = MsgType_t.TUPLE_PUT_RES;
            
            reply();
        }
        
        l2fwd.apply();
        ig_tm_md.bypass_egress = 1;
    }
}


Pipeline(
        IngressParser(),
        Ingress(),
        IngressDeparser(),
        EmptyEgressParser(),
        EmptyEgress(),
        EmptyEgressDeparser()
) pipe;

Switch(pipe) main;
