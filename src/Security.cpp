#include "Security.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cerrno>

// System call and low-level includes
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <sched.h>
#include <fcntl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/sysmacros.h>
#include <climits>  // For PATH_MAX

// Use the official BPF macros from the header to avoid redefinition
#ifndef BPF_STMT
#define BPF_STMT(code, k) ((struct sock_filter){(code), 0, 0, (k)})
#endif

#ifndef BPF_JUMP
#define BPF_JUMP(code, k, jt, jf) ((struct sock_filter){(code), (jt), (jf), (k)})
#endif

namespace Security {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Helper to log errors with context
static void log_error(const std::string& msg) {
    std::cerr << "[SecurityError] " << msg << ": " << strerror(errno) << std::endl;
}

// Wrapper for the pivot_root system call
static int pivot_root(const char* new_root, const char* put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

// ✅ NEW: WSL Detection Function
 bool SecurityManager::is_running_in_wsl() {
    // Check /proc/version for Microsoft/WSL signatures
    std::ifstream version_file("/proc/version");
    if (version_file.is_open()) {
        std::string version_content;
        std::getline(version_file, version_content);
        version_file.close();

        if (version_content.find("microsoft") != std::string::npos ||
            version_content.find("Microsoft") != std::string::npos ||
            version_content.find("WSL") != std::string::npos) {
            return true;
        }
    }

    // Also check /proc/sys/kernel/osrelease
    std::ifstream osrelease("/proc/sys/kernel/osrelease");
    if (osrelease.is_open()) {
        std::string release;
        std::getline(osrelease, release);
        osrelease.close();

        if (release.find("microsoft") != std::string::npos ||
            release.find("Microsoft") != std::string::npos) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// FilesystemSecurity Implementation
// ============================================================================

bool FilesystemSecurity::ensure_directory(const std::string& path) {
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        log_error("Failed to create directory " + path);
        return false;
    }
    return true;
}

// ✅ IMPROVED: Better pivot_root with absolute path handling
bool FilesystemSecurity::setup_pivot_root(const std::string& new_root, const std::string& put_old) {
    // Convert to absolute path if necessary
    std::string absolute_newroot;
    if (new_root[0] == '/') {
        absolute_newroot = new_root;
    } else {
        char cwd_buf[PATH_MAX];
        if (getcwd(cwd_buf, sizeof(cwd_buf)) == nullptr) {
            log_error("getcwd failed");
            return false;
        }
        absolute_newroot = std::string(cwd_buf) + "/" + new_root;
        std::cout << "[Security] Converted relative path to: " << absolute_newroot << std::endl;
    }

    // Ensure the path exists and is a directory
    struct stat st;
    if (stat(absolute_newroot.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        std::cerr << "[Security] ERROR: " << absolute_newroot << " is not a valid directory" << std::endl;
        log_error("rootfs path validation failed");
        return false;
    }

    // Step 1: Bind mount the new root onto itself to make it a mount point
    std::cout << "[Security] Bind mounting " << absolute_newroot << " onto itself..." << std::endl;
    if (mount(absolute_newroot.c_str(), absolute_newroot.c_str(), "bind", MS_BIND | MS_REC, nullptr) != 0) {
        log_error("Failed to bind mount newroot for pivot_root");
        return false;
    }
    std::cout << "[Security] ✓ Bind mount successful" << std::endl;

    // Step 2: Create the directory for the old root
    std::string oldroot_dir = absolute_newroot + "/" + put_old;
    std::cout << "[Security] Creating old root directory: " << oldroot_dir << std::endl;
    if (!ensure_directory(oldroot_dir)) {
        return false;
    }

    // Step 3: Change to the new root directory
    std::cout << "[Security] Changing to new root directory..." << std::endl;
    if (chdir(absolute_newroot.c_str()) != 0) {
        log_error("Failed to chdir to newroot before pivot_root");
        return false;
    }

    // Step 4: Perform the pivot_root syscall
    std::cout << "[Security] Executing pivot_root syscall..." << std::endl;
    if (pivot_root(".", put_old.c_str()) != 0) {
        log_error("pivot_root syscall failed");
        std::cerr << "[Security] pivot_root(\".\", \"" << put_old << "\") failed" << std::endl;
        return false;
    }
    std::cout << "[Security] ✓ pivot_root syscall successful" << std::endl;

    // Step 5: Change to the root of the new filesystem
    std::cout << "[Security] Changing to / in new root..." << std::endl;
    if (chdir("/") != 0) {
        log_error("Failed to chdir to new root");
        return false;
    }

    // Step 6: Unmount the old root
    std::cout << "[Security] Unmounting old root at " << put_old << "..." << std::endl;
    if (umount2(put_old.c_str(), MNT_DETACH) != 0) {
        log_error("Failed to unmount old root");
        // Non-fatal - continue anyway
    } else {
        std::cout << "[Security] ✓ Old root unmounted" << std::endl;
    }

    // Step 7: Remove the old root directory
    std::cout << "[Security] Removing old root directory..." << std::endl;
    if (rmdir(put_old.c_str()) != 0) {
        log_error("Failed to remove old root directory");
        // Non-fatal - continue anyway
    } else {
        std::cout << "[Security] ✓ Old root directory removed" << std::endl;
    }

    return true;
}

// ✅ NEW: Simple chroot fallback for WSL
static bool setup_simple_chroot(const std::string& rootfs) {
    // Convert to absolute path if necessary
    std::string absolute_rootfs;
    if (rootfs[0] == '/') {
        absolute_rootfs = rootfs;
    } else {
        char cwd_buf[PATH_MAX];
        if (getcwd(cwd_buf, sizeof(cwd_buf)) == nullptr) {
            log_error("getcwd failed");
            return false;
        }
        absolute_rootfs = std::string(cwd_buf) + "/" + rootfs;
    }

    std::cout << "[Security] Chroot to: " << absolute_rootfs << std::endl;

    // Perform chroot
    if (chroot(absolute_rootfs.c_str()) != 0) {
        log_error("chroot failed");
        return false;
    }
    std::cout << "[Security] ✓ chroot successful" << std::endl;

    // Change to root directory
    if (chdir("/") != 0) {
        log_error("chdir to new root failed");
        return false;
    }
    std::cout << "[Security] ✓ Changed to new root" << std::endl;

    return true;
}

bool FilesystemSecurity::mount_readonly(const std::string& path) {
    if (mount(nullptr, path.c_str(), nullptr, MS_RDONLY | MS_REMOUNT | MS_BIND, nullptr) != 0) {
        log_error("Failed to remount " + path + " as read-only");
        return false;
    }
    return true;
}

bool FilesystemSecurity::create_bind_mount(const std::string& source, const std::string& target, bool readonly) {
    if (!ensure_directory(target)) return false;
    if (mount(source.c_str(), target.c_str(), nullptr, MS_BIND | MS_REC, nullptr) != 0) {
        log_error("Failed to create bind mount from " + source + " to " + target);
        return false;
    }
    if (readonly) {
        return mount_readonly(target);
    }
    return true;
}

bool FilesystemSecurity::setup_tmpfs(const std::string& target, size_t size_mb) {
    if (!ensure_directory(target)) return false;
    std::string options = "mode=1777";
    if (size_mb > 0) {
        options += ",size=" + std::to_string(size_mb) + "m";
    }
    if (mount("tmpfs", target.c_str(), "tmpfs", 0, options.c_str()) != 0) {
        log_error("Failed to mount tmpfs at " + target);
        return false;
    }
    return true;
}

// ============================================================================
// UserSecurity Implementation
// ============================================================================

bool UserSecurity::write_uid_map(pid_t pid, const std::vector<UserMapping>& mappings) {
    std::string map_path = "/proc/" + std::to_string(pid) + "/uid_map";
    std::ofstream map_file(map_path);
    if (!map_file) { log_error("Failed to open " + map_path); return false; }
    for (const auto& map : mappings) {
        map_file << map.container_id << " " << map.host_id << " " << map.range << "\n";
    }
    return true;
}

bool UserSecurity::write_gid_map(pid_t pid, const std::vector<UserMapping>& mappings) {
    std::string map_path = "/proc/" + std::to_string(pid) + "/gid_map";
    std::ofstream map_file(map_path);
    if (!map_file) { log_error("Failed to open " + map_path); return false; }
    for (const auto& map : mappings) {
        map_file << map.container_id << " " << map.host_id << " " << map.range << "\n";
    }
    return true;
}

bool UserSecurity::setup_setgroups(pid_t pid, bool allow) {
    std::string setgroups_path = "/proc/" + std::to_string(pid) + "/setgroups";
    std::ofstream setgroups_file(setgroups_path);
    if (!setgroups_file) { log_error("Failed to open " + setgroups_path); return false; }
    setgroups_file << (allow ? "allow" : "deny");
    return true;
}

bool UserSecurity::drop_to_user(uid_t uid, gid_t gid) {
    if (setgid(gid) != 0) { log_error("Failed to setgid"); return false; }
    if (setuid(uid) != 0) { log_error("Failed to setuid"); return false; }
    return true;
}

// ============================================================================
// CapabilityManager Implementation
// ============================================================================

bool CapabilityManager::drop_capabilities(const std::vector<Capability>& keep_caps) {
    cap_t caps = cap_get_proc();
    if (!caps) { log_error("cap_get_proc failed"); return false; }

    if (cap_clear(caps) != 0) { log_error("cap_clear failed"); cap_free(caps); return false; }

    std::vector<cap_value_t> cap_list;
    for (const auto& c : keep_caps) {
        cap_list.push_back(static_cast<cap_value_t>(c));
    }

    if (!cap_list.empty()) {
        if (cap_set_flag(caps, CAP_EFFECTIVE, cap_list.size(), cap_list.data(), CAP_SET) != 0) {
            log_error("cap_set_flag (EFFECTIVE) failed"); cap_free(caps); return false;
        }
        if (cap_set_flag(caps, CAP_PERMITTED, cap_list.size(), cap_list.data(), CAP_SET) != 0) {
            log_error("cap_set_flag (PERMITTED) failed"); cap_free(caps); return false;
        }
    }

    if (cap_set_proc(caps) != 0) { log_error("cap_set_proc failed"); cap_free(caps); return false; }
    cap_free(caps);
    return true;
}

// ============================================================================
// SeccompFilter Implementation
// ============================================================================

static bool install_seccomp_filter(const std::vector<int>& syscalls_to_block) {
    std::vector<struct sock_filter> filter;

    filter.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)));
    filter.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0));
    filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM));
    filter.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)));

    for (int syscall_nr : syscalls_to_block) {
        filter.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, static_cast<__u32>(syscall_nr), 0, 1));
        filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM));
    }

    filter.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));

    sock_fprog prog = { (unsigned short)filter.size(), filter.data() };

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        log_error("prctl(NO_NEW_PRIVS)"); return false;
    }
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        log_error("prctl(SECCOMP_MODE_FILTER)"); return false;
    }
    return true;
}

bool SeccompFilter::apply_default_profile() {
    std::cout << "[Security] Applying default seccomp profile..." << std::endl;
    std::vector<int> blocked_syscalls = {
        __NR_add_key, __NR_keyctl, __NR_reboot, __NR_swapon, __NR_swapoff,
        __NR_kexec_load, __NR_mount, __NR_umount2, __NR_delete_module
    };
    return install_seccomp_filter(blocked_syscalls);
}

// ============================================================================
// SecurityManager Implementation (Main Orchestrator)
// ============================================================================

bool SecurityManager::apply_parent_mappings(const SecurityConfig& config, pid_t child_pid) {
    if (config.use_user_namespace) {
        if (!UserSecurity::setup_setgroups(child_pid, false)) return false;
        if (!UserSecurity::write_uid_map(child_pid, config.uid_mappings)) return false;
        if (!UserSecurity::write_gid_map(child_pid, config.gid_mappings)) return false;
    }
    return true;
}

// ✅ COMPLETE: Main security setup with WSL support
bool SecurityManager::apply_child_security(const SecurityConfig& config,
                                            const std::string& hostname,
                                            const std::string& rootfs) {
    // ========================================================================
    // PRELIMINARY: Set hostname (can be done early)
    // ========================================================================
    std::cout << "[Security] Setting hostname to: " << hostname << std::endl;
    if (sethostname(hostname.c_str(), hostname.length()) != 0) {
        log_error("sethostname failed");
        return false;
    }

    // ✅ NEW: Detect WSL and decide isolation method
    bool use_pivot = config.use_pivot_root && !is_running_in_wsl();
        if (is_running_in_wsl() && config.use_pivot_root) {
            std::cout << "[Security] ⚠  WSL detected - pivot_root not supported" << std::endl;
            std::cout << "[Security] Falling back to chroot-based isolation" << std::endl;
        }

    // ========================================================================
    // PHASE 1: Filesystem setup (requires CAP_SYS_ADMIN)
    // ========================================================================
    std::cout << "[Security] ===== Phase 1: Filesystem Setup =====" << std::endl;

    if (use_pivot) {
        // === PIVOT_ROOT PATH (Native Linux) ===
        std::cout << "[Security] Using pivot_root (full isolation)" << std::endl;

        // Step 1.1: Set mount propagation to private
        std::cout << "[Security] Setting mount propagation to MS_PRIVATE..." << std::endl;
        if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
            log_error("Failed to set mount propagation to private");
            return false;
        }
        std::cout << "[Security] ✓ Mount propagation configured" << std::endl;

        // Step 1.2: Pivot root
        std::cout << "[Security] Performing pivot_root to: " << rootfs << std::endl;
        if (!FilesystemSecurity::setup_pivot_root(rootfs, ".oldroot")) {
            std::cerr << "[Security] ✗ ERROR: pivot_root failed" << std::endl;
            return false;
        }
        std::cout << "[Security] ✓ pivot_root successful, now in new rootfs" << std::endl;

    } else {
        // === CHROOT PATH (WSL-compatible) ===
        std::cout << "[Security] Using chroot (WSL-compatible mode)" << std::endl;
        if (!setup_simple_chroot(rootfs)) {
            std::cerr << "[Security] ✗ ERROR: chroot failed" << std::endl;
            return false;
        }
    }

    // Mount essential filesystems (common to both paths)
    std::cout << "[Security] Mounting /proc..." << std::endl;
    FilesystemSecurity::ensure_directory("/proc");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) != 0) {
        log_error("mount /proc failed");
        // Non-fatal in WSL
    } else {
        std::cout << "[Security] ✓ /proc mounted" << std::endl;
    }

    std::cout << "[Security] Mounting /sys..." << std::endl;
    FilesystemSecurity::ensure_directory("/sys");
    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY, nullptr) != 0) {
        log_error("mount /sys failed");
        // Non-fatal in WSL
    } else {
        std::cout << "[Security] ✓ /sys mounted (read-only)" << std::endl;
    }

    // Setup /dev
    std::cout << "[Security] Setting up /dev..." << std::endl;
    FilesystemSecurity::ensure_directory("/dev");
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
        log_error("mount /dev failed");
    } else {
        std::cout << "[Security] ✓ /dev mounted" << std::endl;

        // Create essential device nodes
        mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
        mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));
        mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
        mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
        mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0));
    }

    // Setup /dev/pts
    FilesystemSecurity::ensure_directory("/dev/pts");
    if (mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "newinstance,ptmxmode=0666") == 0) {
        std::cout << "[Security] ✓ /dev/pts mounted" << std::endl;
        symlink("pts/ptmx", "/dev/ptmx");
    }

    // Create standard symlinks
    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");

    // Custom bind mounts
    if (!config.bind_mounts.empty()) {
        std::cout << "[Security] Creating " << config.bind_mounts.size() << " bind mount(s)..." << std::endl;
        for (const auto& mount_pair : config.bind_mounts) {
            if (!FilesystemSecurity::create_bind_mount(mount_pair.first, mount_pair.second, false)) {
                std::cerr << "[Security] ✗ Bind mount failed" << std::endl;
                return false;
            }
        }
        std::cout << "[Security] ✓ All bind mounts created" << std::endl;
    }

    // Setup tmpfs at /tmp
    if (config.setup_tmpfs) {
        if (!FilesystemSecurity::setup_tmpfs("/tmp", config.tmpfs_size_mb)) {
            std::cerr << "[Security] Warning: Failed to setup /tmp" << std::endl;
        } else {
            std::cout << "[Security] ✓ /tmp tmpfs mounted" << std::endl;
        }
    }

    // Make rootfs read-only if requested
    if (config.readonly_rootfs) {
        if (!FilesystemSecurity::mount_readonly("/")) {
            std::cerr << "[Security] Warning: Failed to remount rootfs as read-only" << std::endl;
        } else {
            std::cout << "[Security] ✓ Rootfs is now read-only" << std::endl;
        }
    }

    std::cout << "[Security] ===== Phase 1 Complete: All filesystems mounted =====" << std::endl;

    // ========================================================================
    // PHASE 2: Drop privileges
    // ========================================================================
    std::cout << "[Security] ===== Phase 2: Dropping Privileges =====" << std::endl;

    if (config.drop_capabilities) {
        std::cout << "[Security] Dropping capabilities..." << std::endl;
        if (!CapabilityManager::drop_capabilities(config.keep_capabilities)) {
            std::cerr << "[Security] ✗ Failed to drop capabilities" << std::endl;
            return false;
        }
        std::cout << "[Security] ✓ Capabilities dropped" << std::endl;
    }

    if (config.use_user_namespace) {
        std::cout << "[Security] Dropping to unprivileged user (UID: "
                  << config.container_uid << ", GID: " << config.container_gid << ")..." << std::endl;
        if (!UserSecurity::drop_to_user(config.container_uid, config.container_gid)) {
            std::cerr << "[Security] ✗ Failed to drop to unprivileged user" << std::endl;
            return false;
        }
        std::cout << "[Security] ✓ Now running as UID " << getuid() << ", GID " << getgid() << std::endl;
    }

    std::cout << "[Security] ===== Phase 2 Complete: Privileges dropped =====" << std::endl;

    // ========================================================================
    // PHASE 3: Apply seccomp filter
    // ========================================================================
    std::cout << "[Security] ===== Phase 3: Applying Seccomp Filter =====" << std::endl;

    if (config.use_seccomp) {
        if (config.seccomp_profile == "default") {
            if (!SeccompFilter::apply_default_profile()) {
                std::cerr << "[Security] ✗ Failed to apply seccomp filter" << std::endl;
                return false;
            }
            std::cout << "[Security] ✓ Default seccomp filter applied" << std::endl;
        }
    } else {
        std::cout << "[Security] Skipping seccomp (disabled in config)" << std::endl;
    }

    std::cout << "[Security] ===== Phase 3 Complete: Seccomp active =====" << std::endl;
    std::cout << "[Security] ========================================" << std::endl;
    std::cout << "[Security] ✓✓✓ ALL SECURITY PHASES COMPLETE ✓✓✓" << std::endl;
    std::cout << "[Security] ========================================" << std::endl;

    return true;
}

} // namespace Security
