#include "dpdk.hpp"

#include <algorithm>
#include <fstream>


struct SystemUtils {
    static size_t get_num_hugepages() {
        // Per node: /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
        size_t num_pages = 0;
        std::vector<std::string> files = {
            "/proc/sys/vm/nr_hugepages"};
        for (auto& filename : files) {
            std::ifstream f(filename);
            if (!f) {
                EXIT_WITH_ERROR("Could not open: %s to get hugepages.", filename.c_str());
            }
            size_t num;
            f >> num;
            f.close();
            num_pages += num;
        }

        return num_pages;
    }

    static std::string exec(std::string cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    static bool kernel_module_loaded(std::string name) {
        return exec("lsmod | grep -s " + name).size() > 0;
    }
};


#include "rte_pdump.h"


Dpdk& Dpdk::getInstance() {
    static Dpdk instance;
    if (unlikely(!Dpdk::initialized)) {
        LOG_ERROR("Must initialize Dpdk first using Dpdk::init()");
        throw std::runtime_error("");
    }
    return instance;
}


bool Dpdk::init(CPUSet cpus, size_t mbuf_pool_size, uint32_t masterCore) {
    if (Dpdk::initialized) {
        LOG_ERROR("Trying to re-initialize Dpdk");
        return false;
    }


    size_t num_hugepages = SystemUtils::get_num_hugepages();
    if (num_hugepages == 0) {
        LOG_ERROR("Trying to initialize Dpdk without hugepages.");
        return false;
    }
    LOG_DEBUG("%zu hugepages available.", num_hugepages);


    if (!SystemUtils::kernel_module_loaded("uio_pci_generic")) {
        LOG_ERROR("uio_pci_generic driver is not loaded. Please run modprobe uio_pci_generic.");
        return false;
    }
    LOG_DEBUG("uio_pci_generic driver is loaded.");


    // verify mbuf_pool_size is power of 2 minus 1
    if (!(mbuf_pool_size != 0 && !((mbuf_pool_size + 1) & mbuf_pool_size))) {
        LOG_ERROR("mbuf_pool_size must be a power of two minus one: n = (2^q - 1). It's currently: %zu", mbuf_pool_size);
        return false;
    }
    Dpdk::mbuf_pool_size = mbuf_pool_size;

    if (!cpus.is_available(masterCore)) {
        LOG_ERROR("master_core %d not in cpu-set", masterCore);
        return false;
    }

    std::vector<std::string> args;
    args.push_back("dpdk");
    args.push_back("-n"); // number of memory channels
    args.push_back("2");
    args.push_back("-w");
    args.push_back("0000:01:00.1");
    // args.push_back("--file-prefix");
    // args.push_back("test");
    // args.push_back("--base-virtaddr=0x6aaa2aa0000");
    // args.push_back("--socket-mem");
    // args.push_back("512,0");
    args.push_back("-l"); // number of cores to run on
    args.push_back(cpus.join(","));
    args.push_back("--master-lcore");
    args.push_back(std::to_string(masterCore));

    std::vector<char*> argv;
    for (const auto& arg : args) {
        LOG_DEBUG("Dpdk initialization params: %s", arg.c_str());
        argv.push_back((char*)arg.data());
    }
    argv.push_back(nullptr);


    int ret = rte_eal_init(argv.size() - 1, argv.data());
    if (ret < 0) {
        LOG_ERROR("failed to init the Dpdk EAL");
        return false;
    }

    if (rte_pdump_init() < 0) {
        LOG_ERROR("rte_pdump_init failed");
        return false;
    }
    LOG_DEBUG("rte_pdump_init() success");

    Dpdk::initialized = true;

    auto& dpdk = Dpdk::getInstance();
    dpdk.setDpdkLogLevel(LoggerPP::Normal);

    return dpdk.init_devices(cpus);
}


bool Dpdk::init_devices(CPUSet _cpus) {
    cpus = _cpus;
    cpus.mark_in_use(getDpdkMasterCore());

    uint16_t num_ports = rte_eth_dev_count_avail();
    if (num_ports == 0) {
        LOG_ERROR("Zero Dpdk ports are initialized. Something went wrong while initializing Dpdk");
        return false;
    }
    LOG_DEBUG("Found %d Dpdk ports. Constructing DpdkDevice for each one", num_ports);

    // Initialize a DpdkDevice per port
    dpdk_devices.reserve(num_ports);
    for (uint16_t i = 0; i < num_ports; i++) {
        DpdkDevice* device_ptr = new DpdkDevice(i, mbuf_pool_size);
        auto device = std::shared_ptr<DpdkDevice>(device_ptr);

        LOG_DEBUG("DpdkDevice #%d: Name='%s', PCI-slot='%s', PMD='%s', MAC Addr='%s' Queues='%d/%d'",
                  i,
                  device->getDeviceName().c_str(),
                  device->getPciAddress().c_str(),
                  device->getPMDName().c_str(),
                  device->getMacAddress().toString().c_str(),
                  device->getTotalNumOfRxQueues(),
                  device->getTotalNumOfTxQueues());
        dpdk_devices.push_back(device);
    }

    return true;
}

std::shared_ptr<DpdkDevice> Dpdk::getDeviceByPort(int16_t port_id) const {
    if (!initialized) {
        LOG_ERROR("DpdkDeviceList not initialized");
        return nullptr;
    }
    return dpdk_devices.at(port_id);
}

std::shared_ptr<DpdkDevice> Dpdk::getDeviceByPciAddress(const std::string& pciAddr) const {
    if (!initialized) {
        LOG_ERROR("DpdkDeviceList not initialized");
        return nullptr;
    }

    for (auto& device : dpdk_devices) {
        if (device->getPciAddress() == pciAddr) {
            return device;
        }
    }
    return nullptr;
}


uint32_t Dpdk::getDpdkMasterCore() const {
    return rte_get_master_lcore();
}

void Dpdk::setDpdkLogLevel(LoggerPP::LogLevel logLevel) {
    if (logLevel == LoggerPP::Normal)
        rte_log_set_global_level(RTE_LOG_NOTICE);
    else // logLevel == LoggerPP::Debug
        rte_log_set_global_level(RTE_LOG_DEBUG);
}

LoggerPP::LogLevel Dpdk::getDpdkLogLevel() const {
    if (rte_log_get_global_level() <= RTE_LOG_NOTICE)
        return LoggerPP::Normal;
    else
        return LoggerPP::Debug;
}

bool Dpdk::writeDpdkLogToFile(FILE* logFile) {
    return rte_openlog_stream(logFile) == 0;
}

// called by rte_eal_remote_launch
int Dpdk::dpdkWorkerThreadStart(void* ptr) {
    DpdkWorkerThread* workerThread = (DpdkWorkerThread*)ptr;
    workerThread->run(rte_lcore_id());
    return 0;
}

bool Dpdk::startDpdkWorkerThreads(std::vector<DpdkWorkerThread*>& workerThreadsVec) {
    if (!initialized) {
        LOG_ERROR("DpdkDeviceList not initialized");
        return false;
    }

    for (auto& core : cpus.available) {
        if (!rte_lcore_is_enabled(core)) {
            LOG_ERROR("Trying to use core #%d which isn't initialized by Dpdk", core);
            return false;
        }
    }

    uint32_t num_cores = cpus.available.size();
    if (num_cores == 0) {
        LOG_ERROR("Number of cores left is 0");
        return false;
    }

    if (num_cores < workerThreadsVec.size()) { // master already removed from core-set
        LOG_ERROR("Number of cores is too small for worker-threads and master-lcore");
        return false;
    }

    for (auto& worker : workerThreadsVec) {
        uint32_t core = cpus.get_free_core();

        int err = rte_eal_remote_launch(dpdkWorkerThreadStart, worker, core);
        if (err != 0) {
            stopDpdkWorkerThreads();
            return false;
        }
        workers.push_back(worker);
    }

    return true;
}

void Dpdk::stopDpdkWorkerThreads() {
    if (workers.empty()) {
        LOG_ERROR("No worker threads were set");
        return;
    }

    for (auto& thread : workers) {
        thread->stop();
        if (rte_eal_wait_lcore(thread->getCoreId()) != 0) {
            LOG_ERROR("Thread on core [%d] could not be stopped", thread->getCoreId());
            continue;
        }
        cpus.mark_unused(thread->getCoreId());
        LOG_DEBUG("Thread on core [%d] stopped", thread->getCoreId());
    }

    workers.clear();

    LOG_DEBUG("All worker threads stopped");
}
