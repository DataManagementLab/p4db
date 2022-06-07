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
    NO_STOCK_0 = 0x03,
    NO_STOCK_1 = 0x04,
    NO_STOCK_2 = 0x05,
    NO_STOCK_3 = 0x06,
    NO_STOCK_4 = 0x07,
    NO_STOCK_5 = 0x08,
    NO_STOCK_6 = 0x09,
    NO_STOCK_7 = 0x0a,
    NO_STOCK_8 = 0x0b,
    NO_STOCK_9 = 0x0c,
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
    
    no_stock_t[14] no_stock_skip;       // Worst case we need to skip 14 instructions
    no_stock_t no_stock_0;
    no_stock_t no_stock_1;
    no_stock_t no_stock_2;
    no_stock_t no_stock_3;
    no_stock_t no_stock_4;
    no_stock_t no_stock_5;
    no_stock_t no_stock_6;
    no_stock_t no_stock_7;
    no_stock_t no_stock_8;
    no_stock_t no_stock_9;
    payment_t payment;
    new_order_t new_order;
    next_type_t next_type;
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
    
    
    state parse_instr {
        transition select(pkt.lookahead<InstrType_t>()) {
            InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT
            InstrType_t.NO_STOCK_0: parse_no_stock_0;
            InstrType_t.NO_STOCK_1: parse_no_stock_1;
            InstrType_t.NO_STOCK_2: parse_no_stock_2;
            InstrType_t.NO_STOCK_3: parse_no_stock_3;
            InstrType_t.NO_STOCK_4: parse_no_stock_4;
            InstrType_t.NO_STOCK_5: parse_no_stock_5;
            InstrType_t.NO_STOCK_6: parse_no_stock_6;
            InstrType_t.NO_STOCK_7: parse_no_stock_7;
            InstrType_t.NO_STOCK_8: parse_no_stock_8;
            InstrType_t.NO_STOCK_9: parse_no_stock_9;
            InstrType_t.PAYMENT: parse_payment;
            InstrType_t.NEW_ORDER: parse_new_order;
            default: reject;
        }
    }
    
    state parse_next_type {
        pkt.extract(hdr.next_type);
        transition accept;
    }
    
    
    state parse_no_stock_0 {
        pkt.extract(hdr.no_stock_0);
        transition parse_instr;
    }
    
    state parse_no_stock_1 {
        pkt.extract(hdr.no_stock_1);
        transition parse_instr;
    }
    
    state parse_no_stock_2 {
        pkt.extract(hdr.no_stock_2);
        transition parse_instr;
    }
    
    state parse_no_stock_3 {
        pkt.extract(hdr.no_stock_3);
        transition parse_instr;
    }
    
    state parse_no_stock_4 {
        pkt.extract(hdr.no_stock_4);
        transition parse_instr;
    }
    
    state parse_no_stock_5 {
        pkt.extract(hdr.no_stock_5);
        transition parse_instr;
    }
    
    state parse_no_stock_6 {
        pkt.extract(hdr.no_stock_6);
        transition parse_instr;
    }
    
    state parse_no_stock_7 {
        pkt.extract(hdr.no_stock_7);
        transition parse_instr;
    }
    
    state parse_no_stock_8 {
        pkt.extract(hdr.no_stock_8);
        transition parse_instr;
    }
    
    state parse_no_stock_9 {
        pkt.extract(hdr.no_stock_9);
        transition parse_instr;
    }
    
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
    
    
    Register<lock_pair, bit<1>>(1, {0, 0}) switch_lock;
    RegisterAction<lock_pair, bit<1>, bit<1>>(switch_lock) try_lock = {
        void apply(inout lock_pair value, out bit<1> rv) {
            if ((hdr.info.locks.left + value.left) == 2) {
                rv = 0;
            } else if ((hdr.info.locks.right + value.right) == 2) {
                rv = 0;
            } else {
                rv = 1;
                value.left  = value.left + hdr.info.locks.left;
                value.right  = value.right + hdr.info.locks.right;
            }
        }
    };
    RegisterAction<lock_pair, bit<1>, bit<1>>(switch_lock) unlock = {
        void apply(inout lock_pair value, out bit<1> rv) {
            value.left = value.left - hdr.info.locks.left;
            value.right = value.right - hdr.info.locks.right;
        }
    };
    RegisterAction<lock_pair, bit<1>, bit<1>>(switch_lock) is_locked = {
        void apply(inout lock_pair value, out bit<1> rv) {
            if (value.left > 0 || value.right > 0) {
                rv = 1;
            } else {
                rv = 0;
            }
        }
    };
    
    
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
    
    
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_payment_w_ytd;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_payment_w_ytd) reg_payment_w_ytd_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.payment.h_amount;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_payment_d_ytd;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_payment_d_ytd) reg_payment_d_ytd_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.payment.h_amount;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_no_d_next_o_id;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_no_d_next_o_id) reg_no_d_next_o_id_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_0) reg_s_ytd_0_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_0.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_0) reg_s_quantity_0_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_0.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_0.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_0) reg_s_order_cnt_0_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_0) reg_s_remote_cnt_0_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_0.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_1) reg_s_ytd_1_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_1.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_1) reg_s_quantity_1_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_1.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_1.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_1) reg_s_order_cnt_1_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_1) reg_s_remote_cnt_1_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_1.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_2) reg_s_ytd_2_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_2.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_2) reg_s_quantity_2_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_2.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_2.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_2) reg_s_order_cnt_2_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_2) reg_s_remote_cnt_2_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_2.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_3) reg_s_ytd_3_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_3.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_3) reg_s_quantity_3_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_3.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_3.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_3) reg_s_order_cnt_3_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_3) reg_s_remote_cnt_3_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_3.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_4) reg_s_ytd_4_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_4.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_4) reg_s_quantity_4_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_4.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_4.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_4) reg_s_order_cnt_4_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_4) reg_s_remote_cnt_4_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_4.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_5) reg_s_ytd_5_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_5.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_5) reg_s_quantity_5_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_5.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_5.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_5) reg_s_order_cnt_5_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_5) reg_s_remote_cnt_5_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_5.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_6) reg_s_ytd_6_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_6.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_6) reg_s_quantity_6_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_6.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_6.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_6) reg_s_order_cnt_6_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_6) reg_s_remote_cnt_6_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_6.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_7) reg_s_ytd_7_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_7.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_7) reg_s_quantity_7_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_7.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_7.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_7) reg_s_order_cnt_7_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_7) reg_s_remote_cnt_7_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_7.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_8) reg_s_ytd_8_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_8.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_8) reg_s_quantity_8_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_8.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_8.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_8) reg_s_order_cnt_8_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_8) reg_s_remote_cnt_8_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_8.is_remote;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_ytd_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_ytd_9) reg_s_ytd_9_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + hdr.no_stock_9.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_quantity_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_quantity_9) reg_s_quantity_9_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            
            if (value < hdr.no_stock_9.ol_quantity + 10) {   // some rewriting to reduce complexity and make it compileable
                value = value + 91;
            }
            value = value - hdr.no_stock_9.ol_quantity;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_order_cnt_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_order_cnt_9) reg_s_order_cnt_9_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + 1;
        }
    };
    Register<bit<32>, bit<16>>(40960, 0x00000000) reg_s_remote_cnt_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_s_remote_cnt_9) reg_s_remote_cnt_9_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = value + (bit<32>) hdr.no_stock_9.is_remote;
        }
    };
    
    
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
        
        // lockfree
        else if (hdr.payment.isValid()) {
            reg_payment_w_ytd_access.execute(hdr.payment.w_id);
            reg_payment_d_ytd_access.execute(hdr.payment.d_id);
            hdr.payment.type = InstrType_t.SKIP;
            reply();
        }
        
        // we can move this here because new-order header is always at the end and only valid if all previous stocks are parsed
        // for which a lock or no lock is needed (transitivity)
        if (hdr.new_order.isValid()) {
            hdr.new_order.d_next_o_id = reg_no_d_next_o_id_access.execute(hdr.new_order.d_id);
            hdr.new_order.type = InstrType_t.SKIP;
        }
        
        
        if (do_access) {
            if (hdr.no_stock_0.isValid()) {
                reg_s_ytd_0_access.execute(hdr.no_stock_0.s_id);
                reg_s_quantity_0_access.execute(hdr.no_stock_0.s_id);
                reg_s_order_cnt_0_access.execute(hdr.no_stock_0.s_id);
                reg_s_remote_cnt_0_access.execute(hdr.no_stock_0.s_id);
                hdr.no_stock_0.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_1.isValid()) {
                reg_s_ytd_1_access.execute(hdr.no_stock_1.s_id);
                reg_s_quantity_1_access.execute(hdr.no_stock_1.s_id);
                reg_s_order_cnt_1_access.execute(hdr.no_stock_1.s_id);
                reg_s_remote_cnt_1_access.execute(hdr.no_stock_1.s_id);
                hdr.no_stock_1.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_2.isValid()) {
                reg_s_ytd_2_access.execute(hdr.no_stock_2.s_id);
                reg_s_quantity_2_access.execute(hdr.no_stock_2.s_id);
                reg_s_order_cnt_2_access.execute(hdr.no_stock_2.s_id);
                reg_s_remote_cnt_2_access.execute(hdr.no_stock_2.s_id);
                hdr.no_stock_2.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_3.isValid()) {
                reg_s_ytd_3_access.execute(hdr.no_stock_3.s_id);
                reg_s_quantity_3_access.execute(hdr.no_stock_3.s_id);
                reg_s_order_cnt_3_access.execute(hdr.no_stock_3.s_id);
                reg_s_remote_cnt_3_access.execute(hdr.no_stock_3.s_id);
                hdr.no_stock_3.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_4.isValid()) {
                reg_s_ytd_4_access.execute(hdr.no_stock_4.s_id);
                reg_s_quantity_4_access.execute(hdr.no_stock_4.s_id);
                reg_s_order_cnt_4_access.execute(hdr.no_stock_4.s_id);
                reg_s_remote_cnt_4_access.execute(hdr.no_stock_4.s_id);
                hdr.no_stock_4.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_5.isValid()) {
                reg_s_ytd_5_access.execute(hdr.no_stock_5.s_id);
                reg_s_quantity_5_access.execute(hdr.no_stock_5.s_id);
                reg_s_order_cnt_5_access.execute(hdr.no_stock_5.s_id);
                reg_s_remote_cnt_5_access.execute(hdr.no_stock_5.s_id);
                hdr.no_stock_5.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_6.isValid()) {
                reg_s_ytd_6_access.execute(hdr.no_stock_6.s_id);
                reg_s_quantity_6_access.execute(hdr.no_stock_6.s_id);
                reg_s_order_cnt_6_access.execute(hdr.no_stock_6.s_id);
                reg_s_remote_cnt_6_access.execute(hdr.no_stock_6.s_id);
                hdr.no_stock_6.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_7.isValid()) {
                reg_s_ytd_7_access.execute(hdr.no_stock_7.s_id);
                reg_s_quantity_7_access.execute(hdr.no_stock_7.s_id);
                reg_s_order_cnt_7_access.execute(hdr.no_stock_7.s_id);
                reg_s_remote_cnt_7_access.execute(hdr.no_stock_7.s_id);
                hdr.no_stock_7.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_8.isValid()) {
                reg_s_ytd_8_access.execute(hdr.no_stock_8.s_id);
                reg_s_quantity_8_access.execute(hdr.no_stock_8.s_id);
                reg_s_order_cnt_8_access.execute(hdr.no_stock_8.s_id);
                reg_s_remote_cnt_8_access.execute(hdr.no_stock_8.s_id);
                hdr.no_stock_8.type = InstrType_t.SKIP;
            }
            if (hdr.no_stock_9.isValid()) {
                reg_s_ytd_9_access.execute(hdr.no_stock_9.s_id);
                reg_s_quantity_9_access.execute(hdr.no_stock_9.s_id);
                reg_s_order_cnt_9_access.execute(hdr.no_stock_9.s_id);
                reg_s_remote_cnt_9_access.execute(hdr.no_stock_9.s_id);
                hdr.no_stock_9.type = InstrType_t.SKIP;
            }
        }
        
        if (!do_recirc) {
            l2fwd.apply();  // send digest if no hit
        }
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
