#include "CgroupManager.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h> // For mkdir
#include <unistd.h>   // For rmdir
#include <cstring>    // For strerror
#include <cerrno>     // For errno

CgroupManager::CgroupManager(const Config& config)
    : config_(config),  // ✅ Initialize reference FIRST
      container_name_(config.hostname),
      cgroup_path_("/sys/fs/cgroup/" + config.hostname) {
    // Constructor body is now empty - all initialization done above
}


bool CgroupManager::setup() {
    // 1. Create the cgroup directory
    if (mkdir(cgroup_path_.c_str(), 0755) != 0 && errno != EEXIST) {
        std::cerr << "Error: Could not create cgroup directory " << cgroup_path_ << ": " << strerror(errno) << std::endl;
        return false;
    }

    // 2. Set memory limit if specified
    if (config_.memory_limit_mb > 0) {
        std::ofstream memory_file(cgroup_path_ + "/memory.max");
        if (!memory_file) {
            std::cerr << "Error: Could not open memory.max" << std::endl;
            return false;
        }
        long long memory_in_bytes = config_.memory_limit_mb * 1024 * 1024;
        memory_file << memory_in_bytes;
        memory_file.close();
    }

    // 3. Set process limit if specified
    if (config_.process_limit > 0) {
        std::ofstream pids_file(cgroup_path_ + "/pids.max");
        if (!pids_file) {
            std::cerr << "Error: Could not open pids.max" << std::endl;
            return false;
        }
        pids_file << config_.process_limit;
        pids_file.close();
    }

    std::cout << "[CgroupManager] Cgroup setup complete at " << cgroup_path_ << std::endl;
    return true;
}

bool CgroupManager::apply(pid_t pid) {
    std::ofstream procs_file(cgroup_path_ + "/cgroup.procs");
    if (!procs_file) {
        std::cerr << "Error: Could not open cgroup.procs: " << strerror(errno) << std::endl;
        return false;
    }
    procs_file << pid;
    procs_file.close();
    std::cout << "[CgroupManager] Applied cgroup to PID " << pid << std::endl;
    return true;
}

void CgroupManager::teardown() {
    if (rmdir(cgroup_path_.c_str()) != 0) {
        // This might fail if processes are still lingering, which is okay to just report.
        std::cerr << "Warning: Could not remove cgroup directory " << cgroup_path_ << ": " << strerror(errno) << std::endl;
    } else {
        std::cout << "[CgroupManager] Cgroup teardown complete." << std::endl;
    }
}
