#ifndef SECURITY_H
#define SECURITY_H

#include <string>
#include <vector>
#include <sys/types.h>

namespace Security {

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct UserMapping {
    uid_t container_id;
    uid_t host_id;
    uid_t range;

    UserMapping(uid_t cid = 0, uid_t hid = 0, uid_t r = 1)
        : container_id(cid), host_id(hid), range(r) {}
};

enum class Capability {
    CAP_CHOWN = 0, CAP_DAC_OVERRIDE = 1, CAP_FOWNER = 3, CAP_FSETID = 4,
    CAP_KILL = 5, CAP_SETGID = 6, CAP_SETUID = 7, CAP_SETPCAP = 8,
    CAP_NET_BIND_SERVICE = 10, CAP_NET_RAW = 13, CAP_SYS_CHROOT = 18,
    CAP_MKNOD = 27, CAP_AUDIT_WRITE = 29, CAP_SETFCAP = 31,
};

struct SecurityConfig {
    // Filesystem
    bool use_pivot_root = true;
    bool readonly_rootfs = true;
    std::vector<std::pair<std::string, std::string>> bind_mounts;
    bool setup_tmpfs = true;
    size_t tmpfs_size_mb = 64;

    // User namespace
    bool use_user_namespace = false;
    std::vector<UserMapping> uid_mappings;
    std::vector<UserMapping> gid_mappings;
    uid_t container_uid = 1000;
    gid_t container_gid = 1000;

    // Capabilities
    bool drop_capabilities = true;
    std::vector<Capability> keep_capabilities;

    // Seccomp
    bool use_seccomp = true;
    std::string seccomp_profile = "default";

    SecurityConfig() {
        uid_mappings.emplace_back(0, 1000, 1);
        gid_mappings.emplace_back(0, 1000, 1);
         keep_capabilities.push_back(Capability::CAP_NET_RAW);
    }

};

// ============================================================================
// HELPER CLASS DEFINITIONS
// ============================================================================

class FilesystemSecurity {
public:
    static bool setup_pivot_root(const std::string& new_root, const std::string& put_old);
    static bool mount_readonly(const std::string& path);
    static bool create_bind_mount(const std::string& source, const std::string& target, bool readonly);
    static bool setup_tmpfs(const std::string& target, size_t size_mb);
    static bool ensure_directory(const std::string& path);
};

class UserSecurity {
public:
    static bool setup_setgroups(pid_t pid, bool allow);
    static bool write_uid_map(pid_t pid, const std::vector<UserMapping>& mappings);
    static bool write_gid_map(pid_t pid, const std::vector<UserMapping>& mappings);
    static bool drop_to_user(uid_t uid, gid_t gid);
};

class CapabilityManager {
public:
    static bool drop_capabilities(const std::vector<Capability>& keep_caps);
};

class SeccompFilter {
public:
    static bool apply_default_profile();
};

// ============================================================================
// MAIN ORCHESTRATOR CLASS
// ============================================================================

class SecurityManager {
public:
    static bool apply_parent_mappings(const SecurityConfig& config, pid_t child_pid);
    static bool apply_child_security(const SecurityConfig& config, const std::string& hostname, const std::string& rootfs);
    static bool is_running_in_wsl() ;
};

} // namespace Security

#endif // SECURITY_H
