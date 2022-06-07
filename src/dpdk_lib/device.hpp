#pragma once

// TODO refactor

#include "enums.hpp"
#include "rte_version.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#if (RTE_VER_YEAR > 17) || (RTE_VER_YEAR == 17 && RTE_VER_MONTH >= 11)
#include "rte_bus_pci.h"
#endif
#include "mbufrawpacket.hpp"
#include "rte_config.h"
#include "rte_cycles.h"
#include "rte_errno.h"
#include "rte_ethdev.h"
#include "rte_malloc.h"
#include "rte_pci.h"


#define DPDK_MAX_RX_QUEUES 16
#define DPDK_MAX_TX_QUEUES 16
#define DPDK_COFIG_SPLIT_HEADER_SIZE 0
#define DPDK_CONFIG_MQ_MODE ETH_RSS

#define MAX_BURST_SIZE 64
#define MAX_NUM_OF_CORES 32


typedef uint32_t CoreMask;


#define LOG_DEBUG(format, ...)                                                                             \
    do {                                                                                                   \
        printf("[%-35s: %-25s: line:%-4d] " format "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define LOG_ERROR(format, ...)                                                                                      \
    do {                                                                                                            \
        fprintf(stderr, "[%-35s: %-25s: line:%-4d] " format "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)


struct MacAddress {
    uint8_t m_Address[6];

    MacAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
        m_Address[0] = a;
        m_Address[1] = b;
        m_Address[2] = c;
        m_Address[3] = d;
        m_Address[4] = e;
        m_Address[5] = f;
    }

    std::string toString() const {
        char str[19];
        snprintf(str, sizeof str, "%02x:%02x:%02x:%02x:%02x:%02x", m_Address[0], m_Address[1], m_Address[2], m_Address[3], m_Address[4], m_Address[5]);
        return std::string(str);
    }

    static MacAddress Zero;
};


/**
 * @typedef OnDpdkPacketsArriveCallback
 * A callback that is called when a burst of packets are captured by DpdkDevice
 * @param[in] packets A pointer to an array of MBufRawPacket
 * @param[in] numOfPackets The length of the array
 * @param[in] threadId The thread/core ID who captured the packets
 * @param[in] device A pointer to the DpdkDevice who captured the packets
 * @param[in] userCookie The user cookie assigned by the user in DpdkDevice#startCaptureSingleThread() or DpdkDevice#startCaptureMultiThreads
 */
// typedef void (*OnDpdkPacketsArriveCallback)(MBufRawPacket* packets, uint32_t numOfPackets, uint8_t threadId, DpdkDevice* device, void* userCookie);

/**
 * @class DpdkDevice
 * Encapsulates a Dpdk port and enables receiving and sending packets using Dpdk as well as getting interface info & status, packet
 * statistics, etc. This class has no public c'tor as it's constructed by DpdkDeviceList during initialization.<BR>
 *
 * __RX/TX queues__: modern NICs provide hardware load-balancing for packets. This means that each packet received by the NIC is hashed
 * by one or more parameter (IP address, port, etc.) and goes into one of several RX queues provided by the NIC. This enables
 * applications to work in a multi-core environment where each core can read packets from different RX queue(s). Same goes for TX
 * queues: it's possible to write packets to different TX queues and the NIC is taking care of sending them to the network.
 * Different NICs provide different number of RX and TX queues. Dpdk supports this capability and enables the user to open the
 * Dpdk port (DpdkDevice) with a single or multiple RX and TX queues. When receiving packets the user can decide from which RX queue
 * to read from, and when transmitting packets the user can decide to which TX queue to send them to. RX/TX queues are configured
 * when opening the DpdkDevice (see openMultiQueues())<BR>
 * 
 * __Capturing packets__: there are two ways to capture packets using DpdkDevice:
 *    - using worker threads (see DpdkDeviceList#startDpdkWorkerThreads() ). When using this method the worker should use the
 *      DpdkDevice#receivePackets() methods to get packets from the DpdkDevice
 *    - by setting a callback which is invoked each time a burst of packets arrives. For more details see 
 *      DpdkDevice#startCaptureSingleThread()
 *
 * __Sending packets:__ DpdkDevice has various methods for sending packets. They enable sending raw packets, parsed packets, etc.
 * for all opened TX queues. Also, Dpdk provides an option to buffer TX packets and send them only when reaching a certain threshold (you
 * can read more about it here: http://dpdk.org/doc/api/rte__ethdev_8h.html#a0e941a74ae1b1b886764bc282458d946). DpdkDevice supports that
 * option as well. See DpdkDevice#sendPackets()<BR>
 *
 * __Get interface info__: DpdkDevice provides all kind of information on the interface/device such as MAC address, MTU, link status,
 * PCI address, PMD (poll-mode-driver) used for this port, etc. In addition it provides RX/TX statistics when receiving or sending
 * packets<BR>
 *
 * __Known limitations:__
 *    - BPF filters are currently not supported by this device (as opposed to other PcapPlusPlus device types. This means that the 
 *      device cannot filter packets before they get to the user
 *    - It's not possible to set or change NIC load-balancing method. Dpdk provides this capability but it's still not
 *      supported by DpdkDevice
 */
class DpdkDevice {
    friend class Dpdk;
    friend class MBufPacket;

    friend struct DPDKPacket;

public:
    /**
     * An enum describing all RSS (Receive Side Scaling) hash functions supported in Dpdk. Notice not all
     * PMDs support all types of hash functions
     */
    enum DpdkRssHashFunction {
        /** IPv4 based flow */
        RSS_IPV4 = 0x1,
        /** Fragmented IPv4 based flow */
        RSS_FRAG_IPV4 = 0x2,
        /** Non-fragmented IPv4 + TCP flow */
        RSS_NONFRAG_IPV4_TCP = 0x4,
        /** Non-fragmented IPv4 + UDP flow */
        RSS_NONFRAG_IPV4_UDP = 0x8,
        /** Non-fragmented IPv4 + SCTP flow */
        RSS_NONFRAG_IPV4_SCTP = 0x10,
        /** Non-fragmented IPv4 + non TCP/UDP/SCTP flow */
        RSS_NONFRAG_IPV4_OTHER = 0x20,
        /** IPv6 based flow */
        RSS_IPV6 = 0x40,
        /** Fragmented IPv6 based flow */
        RSS_FRAG_IPV6 = 0x80,
        /** Non-fragmented IPv6 + TCP flow */
        RSS_NONFRAG_IPV6_TCP = 0x100,
        /** Non-fragmented IPv6 + UDP flow */
        RSS_NONFRAG_IPV6_UDP = 0x200,
        /** Non-fragmented IPv6 + SCTP flow */
        RSS_NONFRAG_IPV6_SCTP = 0x400,
        /** Non-fragmented IPv6 + non TCP/UDP/SCTP flow */
        RSS_NONFRAG_IPV6_OTHER = 0x800,
        /** L2 payload based flow */
        RSS_L2_PAYLOAD = 0x1000,
        /** IPv6 Ex based flow */
        RSS_IPV6_EX = 0x2000,
        /** IPv6 + TCP Ex based flow */
        RSS_IPV6_TCP_EX = 0x4000,
        /** IPv6 + UDP Ex based flow */
        RSS_IPV6_UDP_EX = 0x8000,
        /** Consider device port number as a flow differentiator */
        RSS_PORT = 0x10000,
        /** VXLAN protocol based flow */
        RSS_VXLAN = 0x20000,
        /** GENEVE protocol based flow */
        RSS_GENEVE = 0x40000,
        /** NVGRE protocol based flow */
        RSS_NVGRE = 0x80000
    };

    /**
     * @struct DpdkDeviceConfiguration
     * A struct that contains user configurable parameters for opening a DpdkDevice. All of these parameters have default values so 
     * the user doesn't have to use these parameters or understand exactly what is their effect
     */
    struct DpdkDeviceConfiguration {
        /**
         * When configuring a Dpdk RX queue, Dpdk creates descriptors it will use for receiving packets from the network to this RX queue.
         * This parameter enables to configure the number of descriptors that will be created for each RX queue
         */
        uint16_t receiveDescriptorsNumber;

        /**
         * When configuring a Dpdk TX queue, Dpdk creates descriptors it will use for transmitting packets to the network through this TX queue.
         * This parameter enables to configure the number of descriptors that will be created for each TX queue
         */
        uint16_t transmitDescriptorsNumber;

        /**
         * When configuring a Dpdk device, Dpdk supports to activate the Receive Side Scaling (RSS) feature to distribute traffic between the RX queues
         * This parameter points to an array holding the RSS key to use for hashing specific header fields of received packets.
         * The length of this array should be indicated by rssKeyLength below.
         * Supplying a NULL value causes a default random hash key to be used by the device driver
         */
        uint8_t* rssKey;

        /**
         * This parameter indicates the length in bytes of the array pointed by rssKey.
         * This length will be checked in i40e only. Others assume 40 bytes to be used.
         */
        uint8_t rssKeyLength;

        /**
         * This parameter enables to configure the types of packets to which the RSS hashing must be applied. The value
         * is a mask composed of hash functions described in DpdkRssHashFunction enum. Supplying a value equal to zero
         * disables the RSS feature. Supplying a value equal to -1 enables all hash functions supported by this PMD
         */
        uint64_t rssHashFunction;

        /**
         * A c'tor for this struct
         * @param[in] receiveDescriptorsNumber An optional parameter for defining the number of RX descriptors that will be allocated for each RX queue.
         * Default value is 128
         * @param[in] transmitDescriptorsNumber An optional parameter for defining the number of TX descriptors that will be allocated for each TX queue.
         * Default value is 512
         * @param[in] rssHashFunction This parameter enable to configure the types of packets to which the RSS hashing must be applied.
         * The value provided here should be a mask composed of hash functions described in DpdkRssHashFunction enum. The default value is IPv4 and IPv6
         * @param[in] rssKey A pointer to an array holding the RSS key to use for hashing specific header of received packets. If not
         * specified, there is a default key defined inside DpdkDevice
         * @param[in] rssKeyLength The length in bytes of the array pointed by rssKey. Default value is the length of default rssKey
         */
        DpdkDeviceConfiguration(uint16_t receiveDescriptorsNumber = 128,
                                uint16_t transmitDescriptorsNumber = 512,
                                uint64_t rssHashFunction = RSS_IPV4 | RSS_IPV6,
                                uint8_t* rssKey = DpdkDevice::m_RSSKey,
                                uint8_t rssKeyLength = 40) {
            this->receiveDescriptorsNumber = receiveDescriptorsNumber;
            this->transmitDescriptorsNumber = transmitDescriptorsNumber;
            this->rssKey = rssKey;
            this->rssKeyLength = rssKeyLength;
            this->rssHashFunction = rssHashFunction;
        }
    };

    /**
     * @struct LinkStatus
     * A struct that contains the link status of a DpdkDevice (Dpdk port). Returned from DpdkDevice#getLinkStatus()
     */
    struct LinkStatus {
        enum LinkDuplex {
            FULL_DUPLEX,
            HALF_DUPLEX
        };

        /** True if link is up, false if it's down */
        bool linkUp;
        /** Link speed in Mbps (for example: 10Gbe will show 10000) */
        int linkSpeedMbps;
        /** Link duplex (half/full duplex) */
        LinkDuplex linkDuplex;
    };

    /**
     * @struct RxTxStats
     * A container for RX/TX statistics
     */
    struct RxTxStats {
        /** Total number of packets */
        uint64_t packets;
        /** Total number of successfully received bytes */
        uint64_t bytes;
        /** Packets per second */
        uint64_t packetsPerSec;
        /** Bytes per second */
        uint64_t bytesPerSec;
    };

    /**
     * @struct DpdkDeviceStats
     * A container for DpdkDevice statistics
     */
    struct DpdkDeviceStats {
        /** DpdkDevice ID */
        uint8_t devId;
        /** The timestamp of when the stats were written */
        timespec timestamp;
        /** RX statistics per RX queue */
        RxTxStats rxStats[DPDK_MAX_RX_QUEUES];
        /** TX statistics per TX queue */
        RxTxStats txStats[DPDK_MAX_RX_QUEUES];
        /** RX statistics, aggregated for all RX queues */
        RxTxStats aggregatedRxStats;
        /** TX statistics, aggregated for all TX queues */
        RxTxStats aggregatedTxStats;
        /** Total number of RX packets dropped by H/W because there are no available buffers (i.e RX queues are full) */
        uint64_t rxPacketsDroppedByHW;
        /** Total number of erroneous packets */
        uint64_t rxErroneousPackets;
        /** Total number of RX mbuf allocation failuers */
        uint64_t rxMbufAlocFailed;
        /** Total number of failed transmitted packets. */
        uint64_t txFailedPackets;
    };

    ~DpdkDevice();

    /**
     * @return The device ID (Dpdk port ID)
     */
    int getDeviceId() const { return m_Id; }
    /**
     * @return The device name which is in the format of 'DPDK_[PORT-ID]'
     */
    std::string getDeviceName() const { return std::string(m_DeviceName); }

    /**
     * @return The MAC address of the device (Dpdk port)
     */
    MacAddress getMacAddress() const { return m_MacAddress; }

    /**
     * @return The name of the PMD (poll mode driver) Dpdk is using for this device. You can read about PMDs in the Dpdk documentation:
     * http://dpdk.org/doc/guides/prog_guide/poll_mode_drv.html
     */
    std::string getPMDName() const { return m_PMDName; }

    /**
     * @return The enum type of the PMD (poll mode driver) Dpdk is using for this device. You can read about PMDs in the Dpdk documentation:
     * http://dpdk.org/doc/guides/prog_guide/poll_mode_drv.html
     */
    DpdkPMDType getPMDType() const { return m_PMDType; }

    /**
     * @return The PCI address of the device
     */
    std::string getPciAddress() const { return m_PciAddress; }

    /**
     * @return The device's maximum transmission unit (MTU) in bytes
     */
    uint16_t getMtu() const { return m_DeviceMtu; }

    /**
     * Set a new maximum transmission unit (MTU) for this device
     * @param[in] newMtu The new MTU in bytes
     * @return True if MTU was set successfully, false if operation failed or if PMD doesn't support changing the MTU
     */
    bool setMtu(uint16_t newMtu);

    /**
     * @return True if this device is a virtual interface (such as VMXNET3, 1G/10G virtual function, etc.), false otherwise
     */
    bool isVirtual() const;

    /**
     * Get the link status (link up/down, link speed and link duplex)
     * @param[out] linkStatus A reference to object the result shall be written to
     */
    void getLinkStatus(LinkStatus& linkStatus) const;

    /**
     * @return The core ID used in this context
     */
    uint32_t getCurrentCoreId() const;

    /**
     * @return The number of RX queues currently opened for this device (as configured in openMultiQueues() )
     */
    uint16_t getNumOfOpenedRxQueues() const { return m_NumOfRxQueuesOpened; }

    /**
     * @return The number of TX queues currently opened for this device (as configured in openMultiQueues() )
     */
    uint16_t getNumOfOpenedTxQueues() const { return m_NumOfTxQueuesOpened; }

    /**
     * @return The total number of RX queues available on this device
     */
    uint16_t getTotalNumOfRxQueues() const { return m_TotalAvailableRxQueues; }

    /**
     * @return The total number of TX queues available on this device
     */
    uint16_t getTotalNumOfTxQueues() const { return m_TotalAvailableTxQueues; }


    // void alloc(DPDKPacket *pkt)


    DPDKPacket* allocate() {
        if (unlikely(!m_MBufMempool)) {
            throw std::invalid_argument("Device Mempool not initialized");
        }

        struct rte_mbuf* mbuf = rte_pktmbuf_alloc(m_MBufMempool);

        if (unlikely(!mbuf)) {
            std::stringstream err;
            err << "Could not allocate 1 mbuf on device ";
            err << "getAmountOfFreeMbufs()=" << std::to_string(getAmountOfFreeMbufs()) << ' ';
            err << "getAmountOfMbufsInUse()=" + std::to_string(getAmountOfMbufsInUse());
            throw std::runtime_error(err.str());
        }

        return DPDKPacket::wrap(mbuf);
    }

    void allocate_bulk(DPDKPacket** pkts, uint32_t count) {
        if (unlikely(!m_MBufMempool)) {
            throw std::invalid_argument("Device Mempool not initialized");
        }

        struct rte_mbuf** mbufs = reinterpret_cast<struct rte_mbuf**>(pkts);

        if (rte_pktmbuf_alloc_bulk(m_MBufMempool, mbufs, count) != 0) {
            std::stringstream err;
            err << "Could not allocate " << count << " mbufs on device ";
            err << "getAmountOfFreeMbufs()=" << std::to_string(getAmountOfFreeMbufs()) << ' ';
            err << "getAmountOfMbufsInUse()=" + std::to_string(getAmountOfMbufsInUse());
            throw std::runtime_error(err.str());
        }
    }


    uint16_t receive(DPDKPacket** pkts, uint16_t num_pkts, uint16_t rx_queue_id) {
        if (unlikely(!m_DeviceOpened)) {
            throw std::runtime_error("Device not opened");
        }

        if (unlikely(rx_queue_id >= m_TotalAvailableRxQueues)) {
            throw std::runtime_error("RX queue ID #" + std::to_string(rx_queue_id) + " not available for this device");
        }

        // if (unlikely(pkt == NULL)) {
        //     throw std::runtime_error("Provided address of array to store packets is NULL");
        // }

        if (unlikely(num_pkts > MAX_BURST_SIZE)) {
            throw std::runtime_error("num_pkts > MAX_BURST_SIZE");
        }

        static_assert(sizeof(DPDKPacket) == sizeof(struct rte_mbuf));
        struct rte_mbuf** mbufs = reinterpret_cast<struct rte_mbuf**>(pkts);
        uint16_t nb_pkts = rte_eth_rx_burst(m_Id, rx_queue_id, mbufs, num_pkts);

        if (unlikely(nb_pkts < 0)) {
            throw std::runtime_error("nb_pkts < 0");
        }

        return nb_pkts;
    }


    bool send(DPDKPacket* pkt, uint16_t tx_queue_id) {
        if (unlikely(!m_DeviceOpened)) {
            throw std::runtime_error("Device '" + std::string(m_DeviceName) + "' not opened!");
        }

        if (unlikely(tx_queue_id >= m_NumOfTxQueuesOpened)) {
            throw std::runtime_error("TX queue " + std::to_string(tx_queue_id) + " isn't opened in device");
        }

        struct rte_mbuf* mbuf = pkt->raw();
        uint16_t nb_pkts = rte_eth_tx_burst(m_Id, tx_queue_id, &mbuf, 1);
        return nb_pkts == 1;
    }


    uint16_t send_many(DPDKPacket** pkts, uint16_t nb_pkts, uint16_t tx_queue_id) {
        if (unlikely(!m_DeviceOpened)) {
            throw std::runtime_error("Device '" + std::string(m_DeviceName) + "' not opened!");
        }

        if (unlikely(tx_queue_id >= m_NumOfTxQueuesOpened)) {
            throw std::runtime_error("TX queue " + std::to_string(tx_queue_id) + " isn't opened in device");
        }

        static_assert(sizeof(struct rte_mbuf) == sizeof(DPDKPacket));
        struct rte_mbuf** mbufs = reinterpret_cast<struct rte_mbuf**>(pkts);
        uint16_t nb_pkts_sent = 0;
        do {
            uint16_t sent = rte_eth_tx_burst(m_Id, tx_queue_id, mbufs, nb_pkts - nb_pkts_sent);
            mbufs += sent;
            nb_pkts_sent += sent;
        } while (likely(nb_pkts_sent != nb_pkts));
        return nb_pkts;
    }


    /**
         * Open the Dpdk device. Notice opening the device only makes it ready to use, it doesn't start packet capturing. This method initializes RX and TX queues,
         * configures the Dpdk port and starts it. Call close() to close the device. The device is opened in promiscuous mode
         * @param[in] numOfRxQueuesToOpen Number of RX queues to setup. This number must be smaller or equal to the return value of getTotalNumOfRxQueues()
         * @param[in] numOfTxQueuesToOpen Number of TX queues to setup. This number must be smaller or equal to the return value of getTotalNumOfTxQueues()
         * @param[in] config Optional parameter for defining special port configuration parameters such as number of receive/transmit descriptors. If not set the default
         * parameters will be set (see DpdkDeviceConfiguration)
         * @return True if the device was opened successfully, false if device is already opened, if RX/TX queues configuration failed or of Dpdk port
         * configuration and startup failed
         */
    bool openMultiQueues(uint16_t numOfRxQueuesToOpen, uint16_t numOfTxQueuesToOpen, const DpdkDeviceConfiguration& config = DpdkDeviceConfiguration());

    /**
         * @return The number of free mbufs in device's mbufs pool
         */
    int getAmountOfFreeMbufs() const;

    /**
         * @return The number of mbufs currently in use in device's mbufs pool
         */
    int getAmountOfMbufsInUse() const;

    /**
         * Retrieve RX/TX statistics from device
         * @param[out] stats A reference to a DpdkDeviceStats object where stats will be written into
         */
    void getStatistics(DpdkDeviceStats& stats) const;

    /**
         * Clear device statistics
         */
    void clearStatistics();

    /**
         * Check whether a specific RSS hash function is supported by this device (PMD)
         * @param[in] rssHF RSS hash function to check
         * @return True if this hash function is supported, false otherwise
         */
    bool isDeviceSupportRssHashFunction(DpdkRssHashFunction rssHF) const;

    /**
         * Check whether a mask of RSS hash functions is supported by this device (PMD)
         * @param[in] rssHFMask RSS hash functions mask to check. This mask should be built from values in DpdkRssHashFunction enum
         * @return True if all hash functions in this mask are supported, false otherwise
         */
    bool isDeviceSupportRssHashFunction(uint64_t rssHFMask) const;

    /**
         * @return A mask of all RSS hash functions supported by this device (PMD). This mask is built from values in DpdkRssHashFunction enum.
         * Value of zero means RSS is not supported by this device
         */
    uint64_t getSupportedRssHashFunctions() const;

    //overridden methods

    /**
         * Overridden method from IPcapDevice. It calls openMultiQueues() with 1 RX queue and 1 TX queue.
         * Notice opening the device only makes it ready to use, it doesn't start packet capturing. The device is opened in promiscuous mode
         * @return True if the device was opened successfully, false if device is already opened, if RX/TX queues configuration failed or of Dpdk port
         * configuration and startup failed
         */
    bool open() { return openMultiQueues(1, 1); };

    /**
         * Close the DpdkDevice. When device is closed it's not possible work with it
         */
    void close();

private:
    struct DpdkCoreConfiguration {
        int RxQueueId;
        bool IsCoreInUse;

        void clear() {
            RxQueueId = -1;
            IsCoreInUse = false;
        }

        DpdkCoreConfiguration() : RxQueueId(-1), IsCoreInUse(false) {}
    };


    DpdkDevice(uint16_t port, size_t mbuf_pool_size);

    bool initMemPool(struct rte_mempool*& memPool, const char* mempoolName, size_t mbuf_pool_size);

    bool configurePort(uint8_t numOfRxQueues, uint8_t numOfTxQueues);
    bool initQueues(uint8_t numOfRxQueuesToInit, uint8_t numOfTxQueuesToInit);
    bool startDevice();

    static int dpdkCaptureThreadMain(void* ptr);

    void clearCoreConfiguration();
    bool initCoreConfigurationByCoreMask(CoreMask coreMask);
    int getCoresInUseCount() const;

    void setDeviceInfo();

    uint64_t convertRssHfToDpdkRssHf(uint64_t rssHF) const;
    uint64_t convertDpdkRssHfToRssHf(uint64_t dpdkRssHF) const;

    bool m_DeviceOpened = false;

    char m_DeviceName[30];
    DpdkPMDType m_PMDType;
    std::string m_PMDName;
    std::string m_PciAddress;

    DpdkDeviceConfiguration m_Config;

    int m_Id;
    MacAddress m_MacAddress;
    uint16_t m_DeviceMtu;

    struct rte_mempool* m_MBufMempool;

private:
    DpdkCoreConfiguration m_CoreConfiguration[MAX_NUM_OF_CORES];
    uint16_t m_TotalAvailableRxQueues;
    uint16_t m_TotalAvailableTxQueues;
    uint16_t m_NumOfRxQueuesOpened;
    uint16_t m_NumOfTxQueuesOpened;

    bool m_StopThread;

    bool m_WasOpened;

    // RSS key used by the NIC for load balancing the packets between cores
    static uint8_t m_RSSKey[40];

    mutable DpdkDeviceStats m_PrevStats;
};
