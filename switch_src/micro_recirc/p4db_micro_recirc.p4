#include <core.p4>

#if __TARGET_TOFINO__ == 2
    #include <t2na.p4>
#else
    #include <tna.p4>
#endif


// #define FAST_PATH_RECIRC
// #define BUFFER_PORT_RECIRC


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
    RECIRC = 0x01,
    SKIP = 0b01000000,
    STOP = 0b10000000,
    NEG_STOP = 0b01111111
}

header recirc_t {
    InstrType_t type;   // set to SKIP after processing
}


header next_type_t {
    InstrType_t type;
}


struct header_t {
    ethernet_t ethernet;
    msg_t msg;
    info_t info;
    
    recirc_t[14] recirc_skip;
    recirc_t recirc;
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
            InstrType_t.SKIP &&& InstrType_t.SKIP: parse_recirc_skip;
            default: parse_instr;
        }
    }

    state parse_recirc_skip {
        pkt.extract(hdr.recirc_skip.next);
        transition parse_skips;
    }

    state parse_instr {
        transition select(pkt.lookahead<InstrType_t>()) {
            InstrType_t.STOP &&& InstrType_t.STOP: parse_next_type;  // if first bit is STOP_BIT
            InstrType_t.RECIRC: parse_recirc;
            default: reject;
        }
    }
    
    state parse_next_type {
        pkt.extract(hdr.next_type);
        transition accept;
    }
    
    
    state parse_recirc {
        pkt.extract(hdr.recirc);
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
        size = 16;
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

    Register<bit<1>, bit<1>>(1, 0) recirc_port;
    RegisterAction<bit<1>, bit<1>, bit<1>>(recirc_port) recirc_action = {
        void apply(inout bit<1> value, out bit<1> rv) {
            rv = value;
            value = value + 1;
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

    action recirculate2() {
        ig_tm_md.ucast_egress_port = 172;
        hdr.info.recircs = hdr.info.recircs + 1;
    }
    
    
    
    bool do_recirc = false;
    bool do_access = false;
    
    apply {
        ig_tm_md.bypass_egress = 1;
        if (hdr.info.isValid()) {

            #if defined(BUFFER_PORT_RECIRC)
            bit<1> recirc_port = recirc_action.execute(0);
            #endif
            
            if (hdr.info.multipass == 0) {      // no lock needed, one-go
                if (is_locked.execute(0) == 1) {
                    #if defined(BUFFER_PORT_RECIRC)
                    if (recirc_port == 0) {
                        recirculate(68);
                    } else {
                        recirculate2();
                    }
                    #else
                    recirculate(68);
                    #endif
                    do_recirc = true;
                } else {
                    do_access = true;
                    reply();
                }
                
            } else if (hdr.info.has_lock == 0) {     // first one we lock
                if (try_lock.execute(0) == 0) {
                   #if defined(BUFFER_PORT_RECIRC)
                    if (recirc_port == 0) {
                        recirculate(68);
                    } else {
                        recirculate2();
                    }
                    #else
                    recirculate(68);
                    #endif
                    do_recirc = true;
                } else {
                    hdr.info.has_lock = 1;
                    
                    do_access = true;
                    
                    hdr.next_type.type = (InstrType_t) (hdr.next_type.type & InstrType_t.NEG_STOP);     // remove stop bit
                    do_recirc = true;
                    #if defined(FAST_PATH_RECIRC)
                    recirculate_fast();
                    #else
                    recirculate(68);
                    #endif
                }
            } else if (hdr.next_type.type == InstrType_t.STOP) {    // last one we unlock       // TODO fix the same in smallbank
                unlock.execute(0);
                hdr.info.has_lock = 0;
                
                do_access = true;
                reply();
            } else {                                // we still have lock and keep it
                do_access = true;
                
                hdr.next_type.type = (InstrType_t) (hdr.next_type.type & InstrType_t.NEG_STOP);     // remove stop bit
                do_recirc = true;
                #if defined(FAST_PATH_RECIRC)
                recirculate_fast();
                #else
                recirculate(68);
                #endif
            }
        }
       
        
        if (do_access) {
            if (hdr.recirc.isValid()) {
                hdr.recirc.type = (InstrType_t) (hdr.recirc.type | InstrType_t.SKIP);
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
