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
    BALANCE_0 = 0x00,
    DEPOSIT_CHECKING_0 = 0x10,
    TRANSACT_SAVING_0 = 0x20,
    AMALGAMATE_0 = 0x30,
    BALANCE_1 = 0x01,
    DEPOSIT_CHECKING_1 = 0x11,
    TRANSACT_SAVING_1 = 0x21,
    AMALGAMATE_1 = 0x31,
    BALANCE_2 = 0x02,
    DEPOSIT_CHECKING_2 = 0x12,
    TRANSACT_SAVING_2 = 0x22,
    AMALGAMATE_2 = 0x32,
    BALANCE_3 = 0x03,
    DEPOSIT_CHECKING_3 = 0x13,
    TRANSACT_SAVING_3 = 0x23,
    AMALGAMATE_3 = 0x33,
    BALANCE_4 = 0x04,
    DEPOSIT_CHECKING_4 = 0x14,
    TRANSACT_SAVING_4 = 0x24,
    AMALGAMATE_4 = 0x34,
    BALANCE_5 = 0x05,
    DEPOSIT_CHECKING_5 = 0x15,
    TRANSACT_SAVING_5 = 0x25,
    AMALGAMATE_5 = 0x35,
    BALANCE_6 = 0x06,
    DEPOSIT_CHECKING_6 = 0x16,
    TRANSACT_SAVING_6 = 0x26,
    AMALGAMATE_6 = 0x36,
    BALANCE_7 = 0x07,
    DEPOSIT_CHECKING_7 = 0x17,
    TRANSACT_SAVING_7 = 0x27,
    AMALGAMATE_7 = 0x37,
    BALANCE_8 = 0x08,
    DEPOSIT_CHECKING_8 = 0x18,
    TRANSACT_SAVING_8 = 0x28,
    AMALGAMATE_8 = 0x38,
    BALANCE_9 = 0x09,
    DEPOSIT_CHECKING_9 = 0x19,
    TRANSACT_SAVING_9 = 0x29,
    AMALGAMATE_9 = 0x39,
    ABORT = 0x80,
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
    
    deposit_checking_t deposit_checking_skip;
    balance_t balance_0;
    deposit_checking_t deposit_checking_0;
    transact_saving_t transact_saving_0;
    amalgamate_t amalgamate_0;
    balance_t balance_1;
    deposit_checking_t deposit_checking_1;
    transact_saving_t transact_saving_1;
    amalgamate_t amalgamate_1;
    balance_t balance_2;
    deposit_checking_t deposit_checking_2;
    transact_saving_t transact_saving_2;
    amalgamate_t amalgamate_2;
    balance_t balance_3;
    deposit_checking_t deposit_checking_3;
    transact_saving_t transact_saving_3;
    amalgamate_t amalgamate_3;
    balance_t balance_4;
    deposit_checking_t deposit_checking_4;
    transact_saving_t transact_saving_4;
    amalgamate_t amalgamate_4;
    balance_t balance_5;
    deposit_checking_t deposit_checking_5;
    transact_saving_t transact_saving_5;
    amalgamate_t amalgamate_5;
    balance_t balance_6;
    deposit_checking_t deposit_checking_6;
    transact_saving_t transact_saving_6;
    amalgamate_t amalgamate_6;
    balance_t balance_7;
    deposit_checking_t deposit_checking_7;
    transact_saving_t transact_saving_7;
    amalgamate_t amalgamate_7;
    balance_t balance_8;
    deposit_checking_t deposit_checking_8;
    transact_saving_t transact_saving_8;
    amalgamate_t amalgamate_8;
    balance_t balance_9;
    deposit_checking_t deposit_checking_9;
    transact_saving_t transact_saving_9;
    amalgamate_t amalgamate_9;
    
    next_type_t next_type;
}

struct metadata_t {}

/************************************************************/
// UTILS
/************************************************************/
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
            default: parse_instr;
        }
    }
    
    state set_high_prio {
        parser_prio.set(7);
        transition parse_instr;
    }
    
    state parse_instr {
        transition select(pkt.lookahead<InstrType_t>()) {
            InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT
            InstrType_t.SKIP &&& InstrType_t.SKIP: parse_deposit_checking_skip;  // if SKIP_BIT is set
            InstrType_t.BALANCE_0: parse_balance_0;
            InstrType_t.DEPOSIT_CHECKING_0: parse_deposit_checking_0;
            InstrType_t.TRANSACT_SAVING_0: parse_transact_saving_0;
            InstrType_t.AMALGAMATE_0: parse_amalgamate_0;
            InstrType_t.BALANCE_1: parse_balance_1;
            InstrType_t.DEPOSIT_CHECKING_1: parse_deposit_checking_1;
            InstrType_t.TRANSACT_SAVING_1: parse_transact_saving_1;
            InstrType_t.AMALGAMATE_1: parse_amalgamate_1;
            InstrType_t.BALANCE_2: parse_balance_2;
            InstrType_t.DEPOSIT_CHECKING_2: parse_deposit_checking_2;
            InstrType_t.TRANSACT_SAVING_2: parse_transact_saving_2;
            InstrType_t.AMALGAMATE_2: parse_amalgamate_2;
            InstrType_t.BALANCE_3: parse_balance_3;
            InstrType_t.DEPOSIT_CHECKING_3: parse_deposit_checking_3;
            InstrType_t.TRANSACT_SAVING_3: parse_transact_saving_3;
            InstrType_t.AMALGAMATE_3: parse_amalgamate_3;
            InstrType_t.BALANCE_4: parse_balance_4;
            InstrType_t.DEPOSIT_CHECKING_4: parse_deposit_checking_4;
            InstrType_t.TRANSACT_SAVING_4: parse_transact_saving_4;
            InstrType_t.AMALGAMATE_4: parse_amalgamate_4;
            InstrType_t.BALANCE_5: parse_balance_5;
            InstrType_t.DEPOSIT_CHECKING_5: parse_deposit_checking_5;
            InstrType_t.TRANSACT_SAVING_5: parse_transact_saving_5;
            InstrType_t.AMALGAMATE_5: parse_amalgamate_5;
            InstrType_t.BALANCE_6: parse_balance_6;
            InstrType_t.DEPOSIT_CHECKING_6: parse_deposit_checking_6;
            InstrType_t.TRANSACT_SAVING_6: parse_transact_saving_6;
            InstrType_t.AMALGAMATE_6: parse_amalgamate_6;
            InstrType_t.BALANCE_7: parse_balance_7;
            InstrType_t.DEPOSIT_CHECKING_7: parse_deposit_checking_7;
            InstrType_t.TRANSACT_SAVING_7: parse_transact_saving_7;
            InstrType_t.AMALGAMATE_7: parse_amalgamate_7;
            InstrType_t.BALANCE_8: parse_balance_8;
            InstrType_t.DEPOSIT_CHECKING_8: parse_deposit_checking_8;
            InstrType_t.TRANSACT_SAVING_8: parse_transact_saving_8;
            InstrType_t.AMALGAMATE_8: parse_amalgamate_8;
            InstrType_t.BALANCE_9: parse_balance_9;
            InstrType_t.DEPOSIT_CHECKING_9: parse_deposit_checking_9;
            InstrType_t.TRANSACT_SAVING_9: parse_transact_saving_9;
            InstrType_t.AMALGAMATE_9: parse_amalgamate_9;
            default: reject;
        }
    }
    
    state parse_next_type {
        pkt.extract(hdr.next_type);
        transition accept;
    }
    
    state parse_deposit_checking_skip {
        pkt.extract(hdr.deposit_checking_skip);
        transition parse_instr;
    }
    state parse_balance_0 {
        pkt.extract(hdr.balance_0);
        transition parse_instr;
    }
    state parse_deposit_checking_0 {
        pkt.extract(hdr.deposit_checking_0);
        transition parse_instr;
    }
    state parse_transact_saving_0 {
        pkt.extract(hdr.transact_saving_0);
        transition parse_instr;
    }
    state parse_amalgamate_0 {
        pkt.extract(hdr.amalgamate_0);
        transition parse_instr;
    }
    state parse_balance_1 {
        pkt.extract(hdr.balance_1);
        transition parse_instr;
    }
    state parse_deposit_checking_1 {
        pkt.extract(hdr.deposit_checking_1);
        transition parse_instr;
    }
    state parse_transact_saving_1 {
        pkt.extract(hdr.transact_saving_1);
        transition parse_instr;
    }
    state parse_amalgamate_1 {
        pkt.extract(hdr.amalgamate_1);
        transition parse_instr;
    }
    state parse_balance_2 {
        pkt.extract(hdr.balance_2);
        transition parse_instr;
    }
    state parse_deposit_checking_2 {
        pkt.extract(hdr.deposit_checking_2);
        transition parse_instr;
    }
    state parse_transact_saving_2 {
        pkt.extract(hdr.transact_saving_2);
        transition parse_instr;
    }
    state parse_amalgamate_2 {
        pkt.extract(hdr.amalgamate_2);
        transition parse_instr;
    }
    state parse_balance_3 {
        pkt.extract(hdr.balance_3);
        transition parse_instr;
    }
    state parse_deposit_checking_3 {
        pkt.extract(hdr.deposit_checking_3);
        transition parse_instr;
    }
    state parse_transact_saving_3 {
        pkt.extract(hdr.transact_saving_3);
        transition parse_instr;
    }
    state parse_amalgamate_3 {
        pkt.extract(hdr.amalgamate_3);
        transition parse_instr;
    }
    state parse_balance_4 {
        pkt.extract(hdr.balance_4);
        transition parse_instr;
    }
    state parse_deposit_checking_4 {
        pkt.extract(hdr.deposit_checking_4);
        transition parse_instr;
    }
    state parse_transact_saving_4 {
        pkt.extract(hdr.transact_saving_4);
        transition parse_instr;
    }
    state parse_amalgamate_4 {
        pkt.extract(hdr.amalgamate_4);
        transition parse_instr;
    }
    state parse_balance_5 {
        pkt.extract(hdr.balance_5);
        transition parse_instr;
    }
    state parse_deposit_checking_5 {
        pkt.extract(hdr.deposit_checking_5);
        transition parse_instr;
    }
    state parse_transact_saving_5 {
        pkt.extract(hdr.transact_saving_5);
        transition parse_instr;
    }
    state parse_amalgamate_5 {
        pkt.extract(hdr.amalgamate_5);
        transition parse_instr;
    }
    state parse_balance_6 {
        pkt.extract(hdr.balance_6);
        transition parse_instr;
    }
    state parse_deposit_checking_6 {
        pkt.extract(hdr.deposit_checking_6);
        transition parse_instr;
    }
    state parse_transact_saving_6 {
        pkt.extract(hdr.transact_saving_6);
        transition parse_instr;
    }
    state parse_amalgamate_6 {
        pkt.extract(hdr.amalgamate_6);
        transition parse_instr;
    }
    state parse_balance_7 {
        pkt.extract(hdr.balance_7);
        transition parse_instr;
    }
    state parse_deposit_checking_7 {
        pkt.extract(hdr.deposit_checking_7);
        transition parse_instr;
    }
    state parse_transact_saving_7 {
        pkt.extract(hdr.transact_saving_7);
        transition parse_instr;
    }
    state parse_amalgamate_7 {
        pkt.extract(hdr.amalgamate_7);
        transition parse_instr;
    }
    state parse_balance_8 {
        pkt.extract(hdr.balance_8);
        transition parse_instr;
    }
    state parse_deposit_checking_8 {
        pkt.extract(hdr.deposit_checking_8);
        transition parse_instr;
    }
    state parse_transact_saving_8 {
        pkt.extract(hdr.transact_saving_8);
        transition parse_instr;
    }
    state parse_amalgamate_8 {
        pkt.extract(hdr.amalgamate_8);
        transition parse_instr;
    }
    state parse_balance_9 {
        pkt.extract(hdr.balance_9);
        transition parse_instr;
    }
    state parse_deposit_checking_9 {
        pkt.extract(hdr.deposit_checking_9);
        transition parse_instr;
    }
    state parse_transact_saving_9 {
        pkt.extract(hdr.transact_saving_9);
        transition parse_instr;
    }
    state parse_amalgamate_9 {
        pkt.extract(hdr.amalgamate_9);
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
        
        // hdr.info.recircs = hdr.info.recircs + 1;
    }
    
    action recirculate_fast() {
        ig_tm_md.ucast_egress_port = 156;
        // ig_tm_md.bypass_egress = 1;
        
        hdr.info.recircs = hdr.info.recircs + 1;
    }
    
    
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_0) reg_saving_0_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_0) reg_saving_0_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_0) reg_saving_0_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_0.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_0.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_0;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_0) reg_checking_0_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_0) reg_checking_0_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_0) reg_checking_0_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_0.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_1) reg_saving_1_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_1) reg_saving_1_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_1) reg_saving_1_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_1.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_1.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_1;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_1) reg_checking_1_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_1) reg_checking_1_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_1) reg_checking_1_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_1.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_2) reg_saving_2_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_2) reg_saving_2_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_2) reg_saving_2_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_2.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_2.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_2;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_2) reg_checking_2_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_2) reg_checking_2_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_2) reg_checking_2_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_2.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_3) reg_saving_3_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_3) reg_saving_3_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_3) reg_saving_3_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_3.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_3.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_3;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_3) reg_checking_3_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_3) reg_checking_3_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_3) reg_checking_3_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_3.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_4) reg_saving_4_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_4) reg_saving_4_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_4) reg_saving_4_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_4.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_4.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_4;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_4) reg_checking_4_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_4) reg_checking_4_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_4) reg_checking_4_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_4.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_5) reg_saving_5_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_5) reg_saving_5_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_5) reg_saving_5_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_5.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_5.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_5;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_5) reg_checking_5_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_5) reg_checking_5_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_5) reg_checking_5_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_5.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_6) reg_saving_6_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_6) reg_saving_6_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_6) reg_saving_6_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_6.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_6.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_6;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_6) reg_checking_6_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_6) reg_checking_6_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_6) reg_checking_6_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_6.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_7) reg_saving_7_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_7) reg_saving_7_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_7) reg_saving_7_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_7.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_7.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_7;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_7) reg_checking_7_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_7) reg_checking_7_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_7) reg_checking_7_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_7.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_8) reg_saving_8_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_8) reg_saving_8_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_8) reg_saving_8_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_8.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_8.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_8;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_8) reg_checking_8_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_8) reg_checking_8_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_8) reg_checking_8_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_8.checking_bal;
            rv = value;
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000010) reg_saving_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_9) reg_saving_9_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_9) reg_saving_9_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_saving_9) reg_saving_9_transact = {
        void apply(inout bit<32> value, out bit<32> rv) {
            int<32> a = (int<32>) value;        // IMPORTANT casting int<32> needed, checks like a+b < 0x80000000 do not work!!!
            int<32> b = (int<32>) hdr.transact_saving_9.saving_bal;
            
            if (a + b >= 0) {
                value = value + hdr.transact_saving_9.saving_bal;
                // rv = value;
            } else {
                rv = 0xffffffff;    // signal failure
            }
        }
    };
    Register<bit<32>, bit<16>>(8192, 0x00000020) reg_checking_9;
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_9) reg_checking_9_read = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_9) reg_checking_9_zero = {
        void apply(inout bit<32> value, out bit<32> rv) {
            rv = value;
            value = 0;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32>>(reg_checking_9) reg_checking_9_deposit = {
        void apply(inout bit<32> value, out bit<32> rv) {
            value = value + hdr.deposit_checking_9.checking_bal;
            rv = value;
        }
    };
    
    
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
            if (hdr.balance_0.isValid()) {
                hdr.balance_0.saving_bal = reg_saving_0_read.execute(hdr.balance_0.c_id);
                hdr.balance_0.checking_bal = reg_checking_0_read.execute(hdr.balance_0.c_id);
                // hdr.balance_0.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_0.isValid()) {
                hdr.deposit_checking_0.checking_bal = reg_checking_0_deposit.execute(hdr.deposit_checking_0.c_id);
                hdr.deposit_checking_0.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_0.isValid()) {
                hdr.transact_saving_0.saving_bal = reg_saving_0_transact.execute(hdr.transact_saving_0.c_id);
                hdr.transact_saving_0.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_0.isValid()) {
                hdr.amalgamate_0.saving_bal = reg_saving_0_zero.execute(hdr.amalgamate_0.c_id_0);
                hdr.amalgamate_0.checking_bal = reg_checking_0_zero.execute(hdr.amalgamate_0.c_id_0);
                // hdr.transact_saving_0.type = InstrType_t.SKIP;
            }
            if (hdr.balance_1.isValid()) {
                hdr.balance_1.saving_bal = reg_saving_1_read.execute(hdr.balance_1.c_id);
                hdr.balance_1.checking_bal = reg_checking_1_read.execute(hdr.balance_1.c_id);
                // hdr.balance_1.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_1.isValid()) {
                hdr.deposit_checking_1.checking_bal = reg_checking_1_deposit.execute(hdr.deposit_checking_1.c_id);
                hdr.deposit_checking_1.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_1.isValid()) {
                hdr.transact_saving_1.saving_bal = reg_saving_1_transact.execute(hdr.transact_saving_1.c_id);
                hdr.transact_saving_1.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_1.isValid()) {
                hdr.amalgamate_1.saving_bal = reg_saving_1_zero.execute(hdr.amalgamate_1.c_id_0);
                hdr.amalgamate_1.checking_bal = reg_checking_1_zero.execute(hdr.amalgamate_1.c_id_0);
                // hdr.transact_saving_1.type = InstrType_t.SKIP;
            }
            if (hdr.balance_2.isValid()) {
                hdr.balance_2.saving_bal = reg_saving_2_read.execute(hdr.balance_2.c_id);
                hdr.balance_2.checking_bal = reg_checking_2_read.execute(hdr.balance_2.c_id);
                // hdr.balance_2.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_2.isValid()) {
                hdr.deposit_checking_2.checking_bal = reg_checking_2_deposit.execute(hdr.deposit_checking_2.c_id);
                hdr.deposit_checking_2.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_2.isValid()) {
                hdr.transact_saving_2.saving_bal = reg_saving_2_transact.execute(hdr.transact_saving_2.c_id);
                hdr.transact_saving_2.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_2.isValid()) {
                hdr.amalgamate_2.saving_bal = reg_saving_2_zero.execute(hdr.amalgamate_2.c_id_0);
                hdr.amalgamate_2.checking_bal = reg_checking_2_zero.execute(hdr.amalgamate_2.c_id_0);
                // hdr.transact_saving_2.type = InstrType_t.SKIP;
            }
            if (hdr.balance_3.isValid()) {
                hdr.balance_3.saving_bal = reg_saving_3_read.execute(hdr.balance_3.c_id);
                hdr.balance_3.checking_bal = reg_checking_3_read.execute(hdr.balance_3.c_id);
                // hdr.balance_3.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_3.isValid()) {
                hdr.deposit_checking_3.checking_bal = reg_checking_3_deposit.execute(hdr.deposit_checking_3.c_id);
                hdr.deposit_checking_3.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_3.isValid()) {
                hdr.transact_saving_3.saving_bal = reg_saving_3_transact.execute(hdr.transact_saving_3.c_id);
                hdr.transact_saving_3.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_3.isValid()) {
                hdr.amalgamate_3.saving_bal = reg_saving_3_zero.execute(hdr.amalgamate_3.c_id_0);
                hdr.amalgamate_3.checking_bal = reg_checking_3_zero.execute(hdr.amalgamate_3.c_id_0);
                // hdr.transact_saving_3.type = InstrType_t.SKIP;
            }
            if (hdr.balance_4.isValid()) {
                hdr.balance_4.saving_bal = reg_saving_4_read.execute(hdr.balance_4.c_id);
                hdr.balance_4.checking_bal = reg_checking_4_read.execute(hdr.balance_4.c_id);
                // hdr.balance_4.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_4.isValid()) {
                hdr.deposit_checking_4.checking_bal = reg_checking_4_deposit.execute(hdr.deposit_checking_4.c_id);
                hdr.deposit_checking_4.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_4.isValid()) {
                hdr.transact_saving_4.saving_bal = reg_saving_4_transact.execute(hdr.transact_saving_4.c_id);
                hdr.transact_saving_4.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_4.isValid()) {
                hdr.amalgamate_4.saving_bal = reg_saving_4_zero.execute(hdr.amalgamate_4.c_id_0);
                hdr.amalgamate_4.checking_bal = reg_checking_4_zero.execute(hdr.amalgamate_4.c_id_0);
                // hdr.transact_saving_4.type = InstrType_t.SKIP;
            }
            if (hdr.balance_5.isValid()) {
                hdr.balance_5.saving_bal = reg_saving_5_read.execute(hdr.balance_5.c_id);
                hdr.balance_5.checking_bal = reg_checking_5_read.execute(hdr.balance_5.c_id);
                // hdr.balance_5.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_5.isValid()) {
                hdr.deposit_checking_5.checking_bal = reg_checking_5_deposit.execute(hdr.deposit_checking_5.c_id);
                hdr.deposit_checking_5.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_5.isValid()) {
                hdr.transact_saving_5.saving_bal = reg_saving_5_transact.execute(hdr.transact_saving_5.c_id);
                hdr.transact_saving_5.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_5.isValid()) {
                hdr.amalgamate_5.saving_bal = reg_saving_5_zero.execute(hdr.amalgamate_5.c_id_0);
                hdr.amalgamate_5.checking_bal = reg_checking_5_zero.execute(hdr.amalgamate_5.c_id_0);
                // hdr.transact_saving_5.type = InstrType_t.SKIP;
            }
            if (hdr.balance_6.isValid()) {
                hdr.balance_6.saving_bal = reg_saving_6_read.execute(hdr.balance_6.c_id);
                hdr.balance_6.checking_bal = reg_checking_6_read.execute(hdr.balance_6.c_id);
                // hdr.balance_6.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_6.isValid()) {
                hdr.deposit_checking_6.checking_bal = reg_checking_6_deposit.execute(hdr.deposit_checking_6.c_id);
                hdr.deposit_checking_6.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_6.isValid()) {
                hdr.transact_saving_6.saving_bal = reg_saving_6_transact.execute(hdr.transact_saving_6.c_id);
                hdr.transact_saving_6.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_6.isValid()) {
                hdr.amalgamate_6.saving_bal = reg_saving_6_zero.execute(hdr.amalgamate_6.c_id_0);
                hdr.amalgamate_6.checking_bal = reg_checking_6_zero.execute(hdr.amalgamate_6.c_id_0);
                // hdr.transact_saving_6.type = InstrType_t.SKIP;
            }
            if (hdr.balance_7.isValid()) {
                hdr.balance_7.saving_bal = reg_saving_7_read.execute(hdr.balance_7.c_id);
                hdr.balance_7.checking_bal = reg_checking_7_read.execute(hdr.balance_7.c_id);
                // hdr.balance_7.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_7.isValid()) {
                hdr.deposit_checking_7.checking_bal = reg_checking_7_deposit.execute(hdr.deposit_checking_7.c_id);
                hdr.deposit_checking_7.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_7.isValid()) {
                hdr.transact_saving_7.saving_bal = reg_saving_7_transact.execute(hdr.transact_saving_7.c_id);
                hdr.transact_saving_7.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_7.isValid()) {
                hdr.amalgamate_7.saving_bal = reg_saving_7_zero.execute(hdr.amalgamate_7.c_id_0);
                hdr.amalgamate_7.checking_bal = reg_checking_7_zero.execute(hdr.amalgamate_7.c_id_0);
                // hdr.transact_saving_7.type = InstrType_t.SKIP;
            }
            if (hdr.balance_8.isValid()) {
                hdr.balance_8.saving_bal = reg_saving_8_read.execute(hdr.balance_8.c_id);
                hdr.balance_8.checking_bal = reg_checking_8_read.execute(hdr.balance_8.c_id);
                // hdr.balance_8.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_8.isValid()) {
                hdr.deposit_checking_8.checking_bal = reg_checking_8_deposit.execute(hdr.deposit_checking_8.c_id);
                hdr.deposit_checking_8.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_8.isValid()) {
                hdr.transact_saving_8.saving_bal = reg_saving_8_transact.execute(hdr.transact_saving_8.c_id);
                hdr.transact_saving_8.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_8.isValid()) {
                hdr.amalgamate_8.saving_bal = reg_saving_8_zero.execute(hdr.amalgamate_8.c_id_0);
                hdr.amalgamate_8.checking_bal = reg_checking_8_zero.execute(hdr.amalgamate_8.c_id_0);
                // hdr.transact_saving_8.type = InstrType_t.SKIP;
            }
            if (hdr.balance_9.isValid()) {
                hdr.balance_9.saving_bal = reg_saving_9_read.execute(hdr.balance_9.c_id);
                hdr.balance_9.checking_bal = reg_checking_9_read.execute(hdr.balance_9.c_id);
                // hdr.balance_9.type = InstrType_t.SKIP;   // Don't skip, as only one-pass or in combination with other
            }
            else if (hdr.deposit_checking_9.isValid()) {
                hdr.deposit_checking_9.checking_bal = reg_checking_9_deposit.execute(hdr.deposit_checking_9.c_id);
                hdr.deposit_checking_9.type = (InstrType_t) (InstrType_t.DEPOSIT_CHECKING_0 | InstrType_t.SKIP);
            }
            else if (hdr.transact_saving_9.isValid()) {
                hdr.transact_saving_9.saving_bal = reg_saving_9_transact.execute(hdr.transact_saving_9.c_id);
                hdr.transact_saving_9.type = InstrType_t.SKIP;
            }
            else if (hdr.amalgamate_9.isValid()) {
                hdr.amalgamate_9.saving_bal = reg_saving_9_zero.execute(hdr.amalgamate_9.c_id_0);
                hdr.amalgamate_9.checking_bal = reg_checking_9_zero.execute(hdr.amalgamate_9.c_id_0);
                // hdr.transact_saving_9.type = InstrType_t.SKIP;
            }
        }
        
        if (!do_recirc) {
            l2fwd.apply();  // send digest if no hit
        }
    }
}


/************************************************************/
// EGRESS HEADERS
/************************************************************/
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

/************************************************************/
// EGRESS PARSER
/************************************************************/
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

/************************************************************/
// EGRESS
/************************************************************/
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


Pipeline(
        IngressParser(),
        Ingress(),
        IngressDeparser(),
        EgressParser(),
        Egress(),
        EgressDeparser()
) pipe;

Switch(pipe) main;
