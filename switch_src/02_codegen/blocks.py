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
    ${headers}


    /************************************************************/
    // UTILS
    /************************************************************/
    ${utils}


    /************************************************************/
    // PARSER
    /************************************************************/
    ${parser}


    /************************************************************/
    // INGRESS
    /************************************************************/
    ${ingress}


    Pipeline(
        IngressParser(),
        Ingress(),
        IngressDeparser(),
        EmptyEgressParser(),
        EmptyEgress(),
        EmptyEgressDeparser()
    ) pipe;

    Switch(pipe) main;

    '''
