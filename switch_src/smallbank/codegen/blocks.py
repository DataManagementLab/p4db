from .snippet import Snippet


class Register(Snippet):
    '''\
    Register<${type='bit<32>'}, ${idx_type='bit<32>'}>(${size}, ${default_val=0}) ${name};
    '''


class RegisterAction(Snippet):
    '''\
    RegisterAction<${in_type='bit<32>'}, ${idx_type='bit<32>'}, ${out_type='bit<32>'}>(${reg_name}) ${name} = {
        void apply(inout ${in_type='bit<32>'} value, out ${out_type='bit<32>'} rv) {
            ${body}
        }
    };
    '''


class Utils(Snippet):
    '''\
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
    '''


class P4Program(Snippet):
    '''\
    #include <core.p4>

    #if __TARGET_TOFINO__ == 2
        #include <t2na.p4>
    #else
        #include <tna.p4>
    #endif

    /************************************************************/
    // HEADERS
    /************************************************************/
    ${ingress_headers}

    /************************************************************/
    // UTILS
    /************************************************************/
    ${utils}


    /************************************************************/
    // PARSER
    /************************************************************/
    ${ingress_parser}


    /************************************************************/
    // INGRESS
    /************************************************************/
    ${ingress}


    /************************************************************/
    // EGRESS HEADERS
    /************************************************************/
    ${egress_headers}

    /************************************************************/
    // EGRESS PARSER
    /************************************************************/
    ${egress_parser}

    /************************************************************/
    // EGRESS
    /************************************************************/
    ${egress}


    Pipeline(
        IngressParser(),
        Ingress(),
        IngressDeparser(),
        EgressParser(),
        Egress(),
        EgressDeparser()
    ) pipe;

    Switch(pipe) main;

    '''
