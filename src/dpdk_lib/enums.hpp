#pragma once


/**
 * An enum describing all PMD (poll mode driver) types supported by Dpdk. For more info about these PMDs please visit the Dpdk web-site
 */
enum DpdkPMDType {
    /** Unknown PMD type */
    PMD_UNKNOWN,
    /** Link Bonding for 1GbE and 10GbE ports to allow the aggregation of multiple (slave) NICs into a single logical interface*/
    PMD_BOND,
    /** Intel E1000 PMD */
    PMD_E1000EM,
    /** Intel 1GbE PMD */
    PMD_IGB,
    /** Intel 1GbE virtual function PMD */
    PMD_IGBVF,
    /** Cisco enic (UCS Virtual Interface Card) PMD */
    PMD_ENIC,
    /** Intel fm10k PMD */
    PMD_FM10K,
    /** Intel 40GbE PMD */
    PMD_I40E,
    /** Intel 40GbE virtual function PMD */
    PMD_I40EVF,
    /** Intel 10GbE PMD */
    PMD_IXGBE,
    /** Intel 10GbE virtual function PMD */
    PMD_IXGBEVF,
    /** Mellanox ConnectX-3, ConnectX-3 Pro PMD */
    PMD_MLX4,
    /** Null PMD */
    PMD_NULL,
    /** pcap file PMD */
    PMD_PCAP,
    /** ring-based (memory) PMD */
    PMD_RING,
    /** VirtIO PMD */
    PMD_VIRTIO,
    /** VMWare VMXNET3 PMD */
    PMD_VMXNET3,
    /** Xen Project PMD */
    PMD_XENVIRT,
    /** AF_PACKET PMD */
    PMD_AF_PACKET
};