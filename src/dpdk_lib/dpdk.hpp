#pragma once

#include "cpuset.hpp"
#include "device.hpp"
#include "worker_thread.hpp"

#include <memory>
#include <string>
#include <vector>


#ifndef LOG_DEBUG
#define LOG_DEBUG(format, ...)                                                                             \
    do {                                                                                                   \
        printf("[%-35s: %-25s: line:%-4d] " format "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(format, ...)                                                                                      \
    do {                                                                                                            \
        fprintf(stderr, "[%-35s: %-25s: line:%-4d] " format "\n", __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#endif

#ifndef EXIT_WITH_ERROR
#define EXIT_WITH_ERROR(reason, ...)                                \
    do {                                                            \
        printf("Terminated in error: " reason "\n", ##__VA_ARGS__); \
        exit(1);                                                    \
    } while (0)
#endif


class LoggerPP {
public:
    /**
     * An enum representing the log level. Currently 2 log level are supported: Normal and Debug. Normal is the default log level
     */
    enum LogLevel {
        Normal, ///< Normal log level
        Debug   ///< Debug log level
    };
};


/**
 * @class DpdkDeviceList
 * A singleton class that encapsulates Dpdk initialization and holds the list of DpdkDevice instances. As it's a singleton, it has only
 * one active instance doesn't have a public c'tor. This class has several main uses:
 *    - it contains the initDpdk() static method which initializes the Dpdk infrastructure. It should be called once in every application at
 *      its startup process 
 *    - it contains the list of DpdkDevice instances and enables access to them
 *    - it has methods to start and stop worker threads. See more details in startDpdkWorkerThreads() 
 */
class Dpdk {
    static inline bool initialized = false;
    static inline size_t mbuf_pool_size = 0;

    CPUSet cpus;
    std::vector<std::shared_ptr<DpdkDevice>> dpdk_devices;
    std::vector<DpdkWorkerThread*> workers;

    Dpdk() = default;

    bool init_devices(CPUSet cpus);

    static int dpdkWorkerThreadStart(void* ptr);

public:
    ~Dpdk() = default;

    /**
     * As DpdkDeviceList is a singleton, this is the static getter to retrieve its instance. Note that if the static method
     * initDpdk() was not called or returned false this instance won't be initialized and DpdkDevices won't be initialized either
     * @return The singleton instance of DpdkDeviceList
     */
    static Dpdk& getInstance();

    /**
     * A static method that has to be called once at the startup of every application that uses Dpdk. It does several things:
     *    - verifies huge-pages are set and Dpdk kernel module is loaded (these are set by the setup-dpdk.sh external script that
     *      has to be run before application is started)
     *    - initializes the Dpdk infrastructure
     *    - creates DpdkDevice instances for all ports available for Dpdk
     * 
     * @param[in] coreMask The cores to initialize Dpdk with. After initialization, Dpdk will only be able to use these cores
     * for its work. The core mask should have a bit set for every core to use. For example: if the user want to use cores 1,2
     * the core mask should be 6 (binary: 110)
     * @param[in] mbuf_pool_size The mbuf pool size each DpdkDevice will have. This has to be a number which is a power of 2
     * minus 1, for example: 1023 (= 2^10-1) or 4,294,967,295 (= 2^32-1), etc. This is a Dpdk limitation, not PcapPlusPlus.
     * The size of the mbuf pool size dictates how many packets can be handled by the application at the same time. For example: if
     * pool size is 1023 it means that no more than 1023 packets can be handled or stored in application memory at every point in time
     * @param[in] masterCore The core Dpdk will use as master to control all worker thread. The default, unless set otherwise, is 0
     * @return True if initialization succeeded or false if huge-pages or Dpdk kernel driver are not loaded, if mbuf_pool_size
     * isn't power of 2 minus 1, if Dpdk infra initialization failed or if DpdkDevice initialization failed. Anyway, if this method
     * returned false it's impossible to use Dpdk with PcapPlusPlus. You can get some more details about mbufs and pools in 
     * DpdkDevice.h file description or in Dpdk web site
     */
    static bool init(CPUSet cpus, size_t mbuf_pool_size, uint32_t masterCore = 0);

    /**
     * Get a DpdkDevice by port ID
     * @param[in] portId The port ID
     * @return A pointer to the DpdkDevice or NULL if no such device is found
     */
    std::shared_ptr<DpdkDevice> getDeviceByPort(int16_t port_id) const;

    /**
     * Get a DpdkDevice by port PCI address
     * @param[in] pciAddr The port PCI address
     * @return A pointer to the DpdkDevice or NULL if no such device is found
     */
    std::shared_ptr<DpdkDevice> getDeviceByPciAddress(const std::string& pciAddr) const;

    /**
     * @return A vector of all DpdkDevice instances
     */
    const auto& getDpdkDeviceList() const { return dpdk_devices; }

    /**
     * @return Dpdk master core which is the core that initializes the application
     */
    uint32_t getDpdkMasterCore() const;

    /**
     * Change the log level of all modules of Dpdk
     * @param[in] logLevel The log level to set. LoggerPP#Normal is RTE_LOG_NOTICE and LoggerPP#Debug is RTE_LOG_DEBUG
     */
    void setDpdkLogLevel(LoggerPP::LogLevel logLevel);

    /**
     * @return The current Dpdk log level. RTE_LOG_NOTICE and lower are considered as LoggerPP#Normal. RTE_LOG_INFO or RTE_LOG_DEBUG
     * are considered as LoggerPP#Debug
     */
    LoggerPP::LogLevel getDpdkLogLevel() const;

    /**
     * Order Dpdk to write all its logs to a file
     * @param[in] logFile The file to write to
     * @return True if action succeeded, false otherwise
     */
    bool writeDpdkLogToFile(FILE* logFile);

    /**
     * There are two ways to capture packets using DpdkDevice: one of them is using worker threads and the other way is setting
     * a callback which is invoked each time a burst of packets is captured (see DpdkDevice#startCaptureSingleThread() ). This
     * method implements the first way. See a detailed description of workers in DpdkWorkerThread class description. This method
     * gets a vector of workers (classes that implement the DpdkWorkerThread interface) and a core mask and starts a worker thread 
     * on each core (meaning - call the worker's DpdkWorkerThread#run() method). Workers usually run in an endless loop and will 
     * be ordered to stop by calling stopDpdkWorkerThreads().<BR>
     * Note that number of cores in the core mask must be equal to the number of workers. In addition it's impossible to run a
     * worker thread on Dpdk master core, so the core mask shouldn't include the master core (you can find the master core by
     * calling getDpdkMasterCore() ).
     * @param[in] workerThreadsVec A vector of worker instances to run (classes who implement the DpdkWorkerThread interface). 
     * Number of workers in this vector must be equal to the number of cores in the core mask. Notice that the instances of 
     * DpdkWorkerThread shouldn't be freed until calling stopDpdkWorkerThreads() as these instances are running
     * @return True if all worker threads started successfully or false if: Dpdk isn't initialized (initDpdk() wasn't called or
     * returned false), number of cores differs from number of workers, core mask includes Dpdk master core or if one of the 
     * worker threads couldn't be run
     */
    bool startDpdkWorkerThreads(std::vector<DpdkWorkerThread*>& workerThreadsVec);


    // if (!Dpdk::getInstance().start_worker<AppWorkerThread>(devices.at(0), queueQuantity))

    template <class T, typename... Args>
    T* start_worker(Args&&... args) {
        uint32_t core = cpus.get_free_core();
        return start_worker_on_core<T>(core, std::forward<Args>(args)...);
    }

    template <class T, typename... Args>
    T* start_worker_on_core(uint32_t core, Args&&... args) {
        if (!initialized) {
            LOG_ERROR("DpdkDeviceList not initialized");
            return nullptr;
        }

        if (!rte_lcore_is_enabled(core)) {
            LOG_ERROR("Trying to use core #%d which isn't initialized by Dpdk", core);
            return nullptr;
        }

        T* worker = new T{std::forward<Args>(args)...};

        int err = rte_eal_remote_launch(dpdkWorkerThreadStart, worker, core);
        if (err != 0) {
            LOG_ERROR("Failed rte_eal_remote_launch on core: %d", core);
            stopDpdkWorkerThreads();
            return nullptr;
        }
        workers.push_back(worker);

        return worker;
    }

    /**
     * Assuming worker threads are running, this method orders them to stop by calling DpdkWorkerThread#stop(). Then it waits until
     * they stop running
     */
    void stopDpdkWorkerThreads();
};