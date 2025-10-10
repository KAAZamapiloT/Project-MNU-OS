#ifndef SECURITY_H
#define SECURITY_H

#include <string>
#include <vector>
#include <sys/types.h>

namespace Security {

// A mapping for user namespaces (UIDs and GIDs).
struct UserMapping {
    uid_t container_id;  // ID inside the container (e.g., 0 for root)
    uid_t host_id;       // ID on the host system (e.g., 1000 for an unprivileged user)
    uid_t range;         // Number of IDs to map (usually 1)
};

// Linux capabilities that can be managed.
enum class Capability {
    CAP_AUDIT_WRITE = 29,
    CAP_CHOWN = 0,
    CAP_DAC_OVERRIDE = 1,
    CAP_FOWNER = 3,
    CAP_FSETID = 4,
    CAP_KILL = 5,
    CAP_MKNOD = 27,
    CAP_NET_BIND_SERVICE = 10,
    CAP_NET_RAW = 13,
    CAP_SETFCAP = 31,
    CAP_SETGID = 6,
    CAP_SETPCAP = 8,
    CAP_SETUID = 7,
    CAP_SYS_CHROOT = 18,
};

// This struct consolidates all security-related configurations.
struct SecurityConfig {
    // Filesystem Security
    bool use_pivot_root = true;
    bool readonly_rootfs = false;
    bool setup_tmpfs = true;
    size_t tmpfs_size_mb = 64;
    
    // User Security & Namespaces
    bool use_user_namespace = false;
    std::vector<UserMapping> uid_mappings;
    std::vector<UserMapping> gid_mappings;
    uid_t container_uid = 0; // User to become inside the container
    gid_t container_gid = 0; // Group to become inside the container
    
    // Capability Management
    bool drop_capabilities = true;
    std::vector<Capability> keep_capabilities; // Which capabilities to keep
    
    // Seccomp (Secure Computing Mode)
    bool use_seccomp = true;
    std::string seccomp_profile = "default"; // "default" or "strict"
};

// This class orchestrates the application of all security policies.
class SecurityManager {
public:
    // Applies security policies required in the parent process after clone()
    static bool apply_parent_mappings(const SecurityConfig& config, pid_t child_pid);

    // Applies all security policies in the child process before execvp()
    static bool apply_child_security(const SecurityConfig& config, const std::string& rootfs_path);
};

} // namespace Security

#endif // SECURITY_H
