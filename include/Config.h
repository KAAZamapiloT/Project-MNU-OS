#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <map>
#include "Security.h"  // Include security features

struct Config {
    // Basic Configuration
    std::string name;
    std::string rootfs_path;
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> environment;
    
    // Resource Limits
    int memory_limit_mb = 512;
    int cpu_limit_percent = 100;
    int process_limit = 100;
    
    // Namespace Configuration
    bool use_pid_ns = true;
    bool use_net_ns = true;
    bool use_ipc_ns = true;
    bool use_uts_ns = true;
    bool use_mount_ns = true;
    bool use_user_ns = false;  // NEW: User namespace for rootless containers
    
    // Security Configuration (NEW)
    Security::SecurityConfig security;
    
    // Filesystem Security
    bool use_pivot_root = true;        // Use pivot_root instead of chroot
    bool readonly_rootfs = false;       // Make root filesystem read-only
    std::vector<std::pair<std::string, std::string>> bind_mounts;  // Bind mounts
    bool mount_tmpfs = true;            // Mount /tmp as tmpfs
    size_t tmpfs_size_mb = 64;          // tmpfs size limit
    
    // User Security
    bool rootless = false;              // Run container as non-root
    uid_t container_uid = 1000;         // UID inside container
    gid_t container_gid = 1000;         // GID inside container
    std::vector<Security::UserMapping> uid_mappings;  // UID mappings
    std::vector<Security::UserMapping> gid_mappings;  // GID mappings
    
    // Capability Management
    bool drop_caps = true;              // Drop dangerous capabilities
    std::vector<Security::Capability> keep_caps;  // Capabilities to keep
    
    // Seccomp
    bool enable_seccomp = true;         // Enable seccomp filtering
    std::string seccomp_profile = "default";  // "default", "strict", or path
    
    // Hostname
    std::string hostname = "container";
    
    Config() {
        // Initialize default security config
        security.use_pivot_root = true;
        security.readonly_root = false;
        security.setup_tmpfs = true;
        security.tmpfs_size_mb = 64;
        security.use_user_namespace = false;
        security.drop_capabilities = true;
        security.use_seccomp = true;
        security.seccomp_profile = "default";
        
        // Default UID/GID mappings for rootless mode
        // Maps container root (0) to host user (1000)
        uid_mappings.push_back(Security::UserMapping(0, 1000, 1));
        gid_mappings.push_back(Security::UserMapping(0, 1000, 1));
    }
    
    // Helper to sync config with SecurityConfig
    void sync_security_config() {
        security.use_pivot_root = use_pivot_root;
        security.readonly_root = readonly_rootfs;
        security.setup_tmpfs = mount_tmpfs;
        security.tmpfs_size_mb = tmpfs_size_mb;
        security.use_user_namespace = use_user_ns || rootless;
        security.uid_mappings = uid_mappings;
        security.gid_mappings = gid_mappings;
        security.container_uid = container_uid;
        security.container_gid = container_gid;
        security.drop_capabilities = drop_caps;
        security.keep_capabilities = keep_caps;
        security.use_seccomp = enable_seccomp;
        security.seccomp_profile = seccomp_profile;
        security.bind_mounts = bind_mounts;
    }
};

#endif // CONFIG_H