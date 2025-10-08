#ifndef SECURITY_H
#define SECURITY_H

#include <string>
#include <vector>
#include <sys/types.h>

namespace Security {

// ============================================================================
// FILESYSTEM SECURITY
// ============================================================================

/**
 * Setup pivot_root instead of chroot for better isolation
 * This prevents container from accessing the host filesystem
 */
class FilesystemSecurity {
public:
    /**
     * Replace chroot with pivot_root for enhanced security
     * @param new_root Path to new root filesystem
     * @param put_old Path to store old root (will be unmounted)
     * @return true on success
     */
    static bool setup_pivot_root(const std::string& new_root, const std::string& put_old);
    
    /**
     * Mount filesystem as read-only
     * @param path Path to mount point
     * @param remount If true, remount existing mount as read-only
     * @return true on success
     */
    static bool mount_readonly(const std::string& path, bool remount = false);
    
    /**
     * Create bind mount (useful for sharing specific directories)
     * @param source Source path
     * @param target Target path in container
     * @param readonly Mount as read-only
     * @return true on success
     */
    static bool create_bind_mount(const std::string& source, 
                                   const std::string& target, 
                                   bool readonly = false);
    
    /**
     * Setup tmpfs (temporary in-memory filesystem)
     * @param target Mount point
     * @param size_mb Size limit in MB (0 = no limit)
     * @return true on success
     */
    static bool setup_tmpfs(const std::string& target, size_t size_mb = 0);
    
    /**
     * Setup standard container mounts (/proc, /sys, /dev, /tmp)
     * @param rootfs Root filesystem path
     * @param readonly_root Make root filesystem read-only
     * @return true on success
     */
    static bool setup_container_mounts(const std::string& rootfs, bool readonly_root = false);

private:
    static bool ensure_directory(const std::string& path);
};

// ============================================================================
// USER NAMESPACE & SECURITY
// ============================================================================

/**
 * User namespace mapping for rootless containers
 */
struct UserMapping {
    uid_t container_id;  // ID inside container
    uid_t host_id;       // ID on host
    uid_t range;         // Range of IDs to map
    
    UserMapping(uid_t cid = 0, uid_t hid = 0, uid_t r = 1) 
        : container_id(cid), host_id(hid), range(r) {}
};

class UserSecurity {
public:
    /**
     * Create user namespace and setup UID/GID mappings
     * @param uid_maps UID mappings
     * @param gid_maps GID mappings
     * @return true on success
     */
    static bool setup_user_namespace(const std::vector<UserMapping>& uid_maps,
                                     const std::vector<UserMapping>& gid_maps);
    
    /**
     * Drop to non-root user inside container
     * @param uid User ID to switch to
     * @param gid Group ID to switch to
     * @return true on success
     */
    static bool drop_to_user(uid_t uid, gid_t gid);
    
    /**
     * Write UID/GID mappings to /proc/self/uid_map and gid_map
     */
    static bool write_uid_map(pid_t pid, const std::vector<UserMapping>& mappings);
    static bool write_gid_map(pid_t pid, const std::vector<UserMapping>& mappings);
    
    /**
     * Setup setgroups to allow/deny
     */
    static bool setup_setgroups(pid_t pid, bool allow);
};

// ============================================================================
// CAPABILITY MANAGEMENT
// ============================================================================

// Linux capabilities that can be dropped
enum class Capability {
    CAP_CHOWN = 0,
    CAP_DAC_OVERRIDE = 1,
    CAP_FOWNER = 3,
    CAP_FSETID = 4,
    CAP_KILL = 5,
    CAP_SETGID = 6,
    CAP_SETUID = 7,
    CAP_SETPCAP = 8,
    CAP_NET_BIND_SERVICE = 10,
    CAP_NET_RAW = 13,
    CAP_SYS_CHROOT = 18,
    CAP_MKNOD = 27,
    CAP_AUDIT_WRITE = 29,
    CAP_SETFCAP = 31,
    // Add more as needed
};

class CapabilityManager {
public:
    /**
     * Drop all capabilities except those in keep_caps
     * @param keep_caps Capabilities to keep (empty = drop all)
     * @return true on success
     */
    static bool drop_capabilities(const std::vector<Capability>& keep_caps = {});
    
    /**
     * Drop a specific capability
     */
    static bool drop_capability(Capability cap);
    
    /**
     * Get default safe capability set for containers
     */
    static std::vector<Capability> get_default_caps();
    
    /**
     * Drop all capabilities (most restrictive)
     */
    static bool drop_all_capabilities();

private:
    static bool drop_cap_from_set(int cap_value);
};

// ============================================================================
// SECCOMP (Secure Computing Mode)
// ============================================================================

class SeccompFilter {
public:
    /**
     * Apply default seccomp profile
     * Blocks dangerous syscalls like:
     * - reboot, swapon, swapoff
     * - kernel module operations
     * - privileged operations
     * @return true on success
     */
    static bool apply_default_profile();
    
    /**
     * Apply custom seccomp profile from BPF filter
     * @param filter_path Path to BPF filter file
     * @return true on success
     */
    static bool apply_custom_profile(const std::string& filter_path);
    
    /**
     * Apply seccomp strict mode (only read, write, exit, sigreturn allowed)
     * @return true on success
     */
    static bool apply_strict_mode();
    
    /**
     * Check if seccomp is supported on this system
     */
    static bool is_supported();

private:
    static bool install_seccomp_filter();
    static void setup_default_syscall_filter();
};

// ============================================================================
// COMPLETE SECURITY SETUP
// ============================================================================

struct SecurityConfig {
    // Filesystem
    bool use_pivot_root = true;
    bool readonly_root = false;
    std::vector<std::pair<std::string, std::string>> bind_mounts; // source, target
    bool setup_tmpfs = true;
    size_t tmpfs_size_mb = 64;
    
    // User namespace
    bool use_user_namespace = true;
    std::vector<UserMapping> uid_mappings;
    std::vector<UserMapping> gid_mappings;
    uid_t container_uid = 1000;
    gid_t container_gid = 1000;
    
    // Capabilities
    bool drop_capabilities = true;
    std::vector<Capability> keep_capabilities;
    
    // Seccomp
    bool use_seccomp = true;
    std::string seccomp_profile = "default"; // "default", "strict", or path to custom
    
    SecurityConfig() {
        // Default UID mapping: map container root to unprivileged user
        uid_mappings.push_back(UserMapping(0, 1000, 1));
        gid_mappings.push_back(UserMapping(0, 1000, 1));
    }
};

/**
 * Apply all security features in correct order
 */
class SecurityManager {
public:
    /**
     * Apply all security configurations
     * Must be called in specific order for proper isolation
     * @param config Security configuration
     * @param rootfs Root filesystem path
     * @return true if all succeeded
     */
    static bool apply_all_security(const SecurityConfig& config, 
                                   const std::string& rootfs);
    
    /**
     * Apply security in parent process (before clone)
     */
    static bool apply_parent_security(const SecurityConfig& config);
    
    /**
     * Apply security in child process (after clone)
     */
    static bool apply_child_security(const SecurityConfig& config, 
                                     const std::string& rootfs);

private:
    static bool validate_config(const SecurityConfig& config);
};

} // namespace Security

#endif // SECURITY_H