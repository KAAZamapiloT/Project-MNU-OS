#include "Security.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cerrno>

// Low-level headers for system calls and kernel interactions
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <linux/seccomp.h>
#include <fcntl.h>
#include <sched.h>

namespace Security {

// Internal helper functions
namespace {
    void log_error(const std::string& msg) {
        std::cerr << "[SecurityError] " << msg << ": " << strerror(errno) << std::endl;
    }

    int pivot_root(const char* new_root, const char* put_old) {
        return syscall(SYS_pivot_root, new_root, put_old);
    }

    bool ensure_directory(const std::string& path) {
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            log_error("Failed to create directory " + path);
            return false;
        }
        return true;
    }
}

// Parent-side logic for user namespaces
bool SecurityManager::apply_parent_mappings(const SecurityConfig& config, pid_t child_pid) {
    if (!config.use_user_namespace) {
        return true; // Nothing to do
    }

    auto write_map = [](const std::string& map_file, const std::vector<UserMapping>& mappings) {
        std::ofstream file(map_file);
        if (!file) {
            log_error("Failed to open " + map_file);
            return false;
        }
        for (const auto& map : mappings) {
            file << map.container_id << " " << map.host_id << " " << map.range << "\n";
        }
        return true;
    };

    // Deny setgroups before writing maps for security
    std::ofstream setgroups_file("/proc/" + std::to_string(child_pid) + "/setgroups");
    if (!setgroups_file) {
        log_error("Failed to open setgroups file");
        return false;
    }
    setgroups_file << "deny";
    setgroups_file.close();

    // Write UID and GID maps
    if (!write_map("/proc/" + std::to_string(child_pid) + "/uid_map", config.uid_mappings)) return false;
    if (!write_map("/proc/" + std::to_string(child_pid) + "/gid_map", config.gid_mappings)) return false;

    return true;
}

// Child-side security orchestration
bool SecurityManager::apply_child_security(const SecurityConfig& config, const std::string& rootfs_path) {
    // 1. Filesystem: Isolate with pivot_root for maximum security
    if (config.use_pivot_root) {
        mount(rootfs_path.c_str(), rootfs_path.c_str(), "bind", MS_BIND | MS_REC, nullptr);
        std::string old_root_dir = rootfs_path + "/.old_root";
        ensure_directory(old_root_dir);
        if (pivot_root(rootfs_path.c_str(), old_root_dir.c_str()) != 0) {
            log_error("pivot_root syscall failed");
            return false;
        }
        chdir("/");
        umount2("/.old_root", MNT_DETACH);
        rmdir("/.old_root");
    } else { // Fallback to chroot
        if (chroot(rootfs_path.c_str()) != 0 || chdir("/") != 0) {
            log_error("chroot fallback failed");
            return false;
        }
    }

    // 2. Mounts: Set up virtual filesystems (/proc, /sys) and tmpfs (/tmp)
    umount2("/proc", MNT_DETACH); // Unmount host /proc if it was inherited
    ensure_directory("/proc");
    mount("proc", "/proc", "proc", 0, nullptr);
    ensure_directory("/sys");
    mount("sysfs", "/sys", "sysfs", 0, nullptr);

    if (config.setup_tmpfs) {
        ensure_directory("/tmp");
        std::string opts = "mode=1777,size=" + std::to_string(config.tmpfs_size_mb) + "m";
        mount("tmpfs", "/tmp", "tmpfs", 0, opts.c_str());
    }

    if (config.readonly_rootfs) {
        mount(nullptr, "/", nullptr, MS_RDONLY | MS_REMOUNT | MS_BIND, nullptr);
    }
    
    // 3. User: Drop to a non-root user inside the container
    if (config.use_user_namespace) {
        if (setgid(config.container_gid) != 0 || setuid(config.container_uid) != 0) {
            log_error("Failed to drop to non-root user");
            return false;
        }
    }

    // 4. Capabilities: Drop privileges to the minimum required set
    if (config.drop_capabilities) {
        cap_t caps = cap_get_proc();
        cap_clear(caps);
        std::vector<cap_value_t> cap_list;
        for (const auto& c : config.keep_capabilities) {
            cap_list.push_back(static_cast<cap_value_t>(c));
        }
        if (!cap_list.empty()) {
            cap_set_flag(caps, CAP_PERMITTED, cap_list.size(), cap_list.data(), CAP_SET);
            cap_set_flag(caps, CAP_EFFECTIVE, cap_list.size(), cap_list.data(), CAP_SET);
            cap_set_flag(caps, CAP_INHERITABLE, cap_list.size(), cap_list.data(), CAP_SET);
        }
        if (cap_set_proc(caps) != 0) log_error("cap_set_proc failed");
        cap_free(caps);
    }
    
    // 5. Seccomp: Apply syscall filter as the last step before exec
    if (config.use_seccomp) {
        prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
        if (config.seccomp_profile == "strict") {
            if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) != 0) log_error("seccomp strict mode failed");
        }
        // A "default" profile would require libseccomp for a BPF filter.
        // For this project, we can treat "default" as no-op or strict.
    }

    return true;
}

} // namespace Security

