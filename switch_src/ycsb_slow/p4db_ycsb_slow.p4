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
    REG_0 = 0x01,
    REG_1 = 0x02,
    REG_2 = 0x03,
    REG_3 = 0x04,
    REG_4 = 0x05,
    REG_5 = 0x06,
    REG_6 = 0x07,
    REG_7 = 0x08,
    REG_8 = 0x09,
    REG_9 = 0x0a,
    REG_10 = 0x0b,
    REG_11 = 0x0c,
    REG_12 = 0x0d,
    REG_13 = 0x0e,
    REG_14 = 0x0f,
    REG_15 = 0x10,
    REG_16 = 0x11,
    REG_17 = 0x12,
    REG_18 = 0x13,
    REG_19 = 0x14,
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
    
    reg_instr_t[7] reg_skip;       // Worst case we need to skip 7 instructions
    reg_instr_t reg_0;
    reg_instr_t reg_1;
    reg_instr_t reg_2;
    reg_instr_t reg_3;
    reg_instr_t reg_4;
    reg_instr_t reg_5;
    reg_instr_t reg_6;
    reg_instr_t reg_7;
    reg_instr_t reg_8;
    reg_instr_t reg_9;
    reg_instr_t reg_10;
    reg_instr_t reg_11;
    reg_instr_t reg_12;
    reg_instr_t reg_13;
    reg_instr_t reg_14;
    reg_instr_t reg_15;
    reg_instr_t reg_16;
    reg_instr_t reg_17;
    reg_instr_t reg_18;
    reg_instr_t reg_19;
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
            InstrType_t.SKIP: parse_reg_skip;
            default: parse_regs;
        }
    }
    
    state parse_reg_skip {
        pkt.extract(hdr.reg_skip.next);
        transition parse_skips;
    }
    
    
    state parse_regs {
        transition select(pkt.lookahead<InstrType_t>()) {
            InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT
            InstrType_t.REG_0: parse_reg_0;
            InstrType_t.REG_1: parse_reg_1;
            InstrType_t.REG_2: parse_reg_2;
            InstrType_t.REG_3: parse_reg_3;
            InstrType_t.REG_4: parse_reg_4;
            InstrType_t.REG_5: parse_reg_5;
            InstrType_t.REG_6: parse_reg_6;
            InstrType_t.REG_7: parse_reg_7;
            InstrType_t.REG_8: parse_reg_8;
            InstrType_t.REG_9: parse_reg_9;
            InstrType_t.REG_10: parse_reg_10;
            InstrType_t.REG_11: parse_reg_11;
            InstrType_t.REG_12: parse_reg_12;
            InstrType_t.REG_13: parse_reg_13;
            InstrType_t.REG_14: parse_reg_14;
            InstrType_t.REG_15: parse_reg_15;
            InstrType_t.REG_16: parse_reg_16;
            InstrType_t.REG_17: parse_reg_17;
            InstrType_t.REG_18: parse_reg_18;
            InstrType_t.REG_19: parse_reg_19;
            default: reject;
        }
    }
    
    state parse_next_type {
        pkt.extract(hdr.next_type);
        transition accept;
    }
    
    
    state parse_reg_0 {
        pkt.extract(hdr.reg_0);
        transition parse_regs;
    }
    
    state parse_reg_1 {
        pkt.extract(hdr.reg_1);
        transition parse_regs;
    }
    
    state parse_reg_2 {
        pkt.extract(hdr.reg_2);
        transition parse_regs;
    }
    
    state parse_reg_3 {
        pkt.extract(hdr.reg_3);
        transition parse_regs;
    }
    
    state parse_reg_4 {
        pkt.extract(hdr.reg_4);
        transition parse_regs;
    }
    
    state parse_reg_5 {
        pkt.extract(hdr.reg_5);
        transition parse_regs;
    }
    
    state parse_reg_6 {
        pkt.extract(hdr.reg_6);
        transition parse_regs;
    }
    
    state parse_reg_7 {
        pkt.extract(hdr.reg_7);
        transition parse_regs;
    }
    
    state parse_reg_8 {
        pkt.extract(hdr.reg_8);
        transition parse_regs;
    }
    
    state parse_reg_9 {
        pkt.extract(hdr.reg_9);
        transition parse_regs;
    }
    
    state parse_reg_10 {
        pkt.extract(hdr.reg_10);
        transition parse_regs;
    }
    
    state parse_reg_11 {
        pkt.extract(hdr.reg_11);
        transition parse_regs;
    }
    
    state parse_reg_12 {
        pkt.extract(hdr.reg_12);
        transition parse_regs;
    }
    
    state parse_reg_13 {
        pkt.extract(hdr.reg_13);
        transition parse_regs;
    }
    
    state parse_reg_14 {
        pkt.extract(hdr.reg_14);
        transition parse_regs;
    }
    
    state parse_reg_15 {
        pkt.extract(hdr.reg_15);
        transition parse_regs;
    }
    
    state parse_reg_16 {
        pkt.extract(hdr.reg_16);
        transition parse_regs;
    }
    
    state parse_reg_17 {
        pkt.extract(hdr.reg_17);
        transition parse_regs;
    }
    
    state parse_reg_18 {
        pkt.extract(hdr.reg_18);
        transition parse_regs;
    }
    
    state parse_reg_19 {
        pkt.extract(hdr.reg_19);
        transition parse_regs;
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
        ig_tm_md.ucast_egress_port = 156;       // TODO why is method from above not working here?
        // ig_tm_md.bypass_egress = 1;
        
        hdr.info.recircs = hdr.info.recircs + 1;
    }
    
    
    Register<bit<32>, bit<16>>(32768, 0x0101) reg_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_0) reg_0_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_0.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_0.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0202) reg_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_1) reg_1_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_1.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_1.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0303) reg_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_2) reg_2_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_2.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_2.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0404) reg_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_3) reg_3_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_3.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_3.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0505) reg_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_4) reg_4_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_4.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_4.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0606) reg_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_5) reg_5_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_5.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_5.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0707) reg_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_6) reg_6_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_6.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_6.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0808) reg_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_7) reg_7_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_7.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_7.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0909) reg_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_8) reg_8_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_8.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_8.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0a0a) reg_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_9) reg_9_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_9.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_9.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0b0b) reg_10;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_10) reg_10_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_10.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_10.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0c0c) reg_11;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_11) reg_11_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_11.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_11.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0d0d) reg_12;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_12) reg_12_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_12.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_12.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0e0e) reg_13;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_13) reg_13_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_13.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_13.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x0f0f) reg_14;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_14) reg_14_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_14.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_14.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x1010) reg_15;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_15) reg_15_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_15.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_15.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x1111) reg_16;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_16) reg_16_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_16.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_16.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x1212) reg_17;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_17) reg_17_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_17.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_17.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x1313) reg_18;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_18) reg_18_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_18.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_18.data;
            }
        }
    };
    Register<bit<32>, bit<16>>(32768, 0x1414) reg_19;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_19) reg_19_access = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            if (hdr.reg_19.op == OPCode_t.WRITE) {     // do comparison here, we want to save Hash Dist Units
                value = hdr.reg_19.data;
            }
        }
    };
    
    
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
            if (hdr.reg_0.isValid()) {
                hdr.reg_0.data = reg_0_access.execute(hdr.reg_0.idx);
                hdr.reg_0.type = InstrType_t.SKIP;
            }
            if (hdr.reg_1.isValid()) {
                hdr.reg_1.data = reg_1_access.execute(hdr.reg_1.idx);
                hdr.reg_1.type = InstrType_t.SKIP;
            }
            if (hdr.reg_2.isValid()) {
                hdr.reg_2.data = reg_2_access.execute(hdr.reg_2.idx);
                hdr.reg_2.type = InstrType_t.SKIP;
            }
            if (hdr.reg_3.isValid()) {
                hdr.reg_3.data = reg_3_access.execute(hdr.reg_3.idx);
                hdr.reg_3.type = InstrType_t.SKIP;
            }
            if (hdr.reg_4.isValid()) {
                hdr.reg_4.data = reg_4_access.execute(hdr.reg_4.idx);
                hdr.reg_4.type = InstrType_t.SKIP;
            }
            if (hdr.reg_5.isValid()) {
                hdr.reg_5.data = reg_5_access.execute(hdr.reg_5.idx);
                hdr.reg_5.type = InstrType_t.SKIP;
            }
            if (hdr.reg_6.isValid()) {
                hdr.reg_6.data = reg_6_access.execute(hdr.reg_6.idx);
                hdr.reg_6.type = InstrType_t.SKIP;
            }
            if (hdr.reg_7.isValid()) {
                hdr.reg_7.data = reg_7_access.execute(hdr.reg_7.idx);
                hdr.reg_7.type = InstrType_t.SKIP;
            }
            if (hdr.reg_8.isValid()) {
                hdr.reg_8.data = reg_8_access.execute(hdr.reg_8.idx);
                hdr.reg_8.type = InstrType_t.SKIP;
            }
            if (hdr.reg_9.isValid()) {
                hdr.reg_9.data = reg_9_access.execute(hdr.reg_9.idx);
                hdr.reg_9.type = InstrType_t.SKIP;
            }
            if (hdr.reg_10.isValid()) {
                hdr.reg_10.data = reg_10_access.execute(hdr.reg_10.idx);
                hdr.reg_10.type = InstrType_t.SKIP;
            }
            if (hdr.reg_11.isValid()) {
                hdr.reg_11.data = reg_11_access.execute(hdr.reg_11.idx);
                hdr.reg_11.type = InstrType_t.SKIP;
            }
            if (hdr.reg_12.isValid()) {
                hdr.reg_12.data = reg_12_access.execute(hdr.reg_12.idx);
                hdr.reg_12.type = InstrType_t.SKIP;
            }
            if (hdr.reg_13.isValid()) {
                hdr.reg_13.data = reg_13_access.execute(hdr.reg_13.idx);
                hdr.reg_13.type = InstrType_t.SKIP;
            }
            if (hdr.reg_14.isValid()) {
                hdr.reg_14.data = reg_14_access.execute(hdr.reg_14.idx);
                hdr.reg_14.type = InstrType_t.SKIP;
            }
            if (hdr.reg_15.isValid()) {
                hdr.reg_15.data = reg_15_access.execute(hdr.reg_15.idx);
                hdr.reg_15.type = InstrType_t.SKIP;
            }
            if (hdr.reg_16.isValid()) {
                hdr.reg_16.data = reg_16_access.execute(hdr.reg_16.idx);
                hdr.reg_16.type = InstrType_t.SKIP;
            }
            if (hdr.reg_17.isValid()) {
                hdr.reg_17.data = reg_17_access.execute(hdr.reg_17.idx);
                hdr.reg_17.type = InstrType_t.SKIP;
            }
            if (hdr.reg_18.isValid()) {
                hdr.reg_18.data = reg_18_access.execute(hdr.reg_18.idx);
                hdr.reg_18.type = InstrType_t.SKIP;
            }
            if (hdr.reg_19.isValid()) {
                hdr.reg_19.data = reg_19_access.execute(hdr.reg_19.idx);
                hdr.reg_19.type = InstrType_t.SKIP;
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
