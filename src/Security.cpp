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
#include <sys/sysmacros.h> // <-- FIX: Added for makedev()

// Use the official BPF macros from the header to avoid redefinition
#ifndef BPF_STMT
#define BPF_STMT(code, k) ((struct sock_filter){(code), 0, 0, (k)})
#endif
#ifndef BPF_JUMP
#define BPF_JUMP(code, k, jt, jf) ((struct sock_filter){(code), (jt), (jf), (k)})
#endif


namespace Security {

// Helper to log errors with context
static void log_error(const std::string& msg) {
    std::cerr << "[SecurityError] " << msg << ": " << strerror(errno) << std::endl;
}

// Wrapper for the pivot_root system call
static int pivot_root(const char* new_root, const char* put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
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

bool FilesystemSecurity::setup_pivot_root(const std::string& new_root, const std::string& put_old) {
    if (mount(new_root.c_str(), new_root.c_str(), "bind", MS_BIND | MS_REC, nullptr) != 0) {
        log_error("Failed to bind mount new_root for pivot_root"); return false;
    }
    std::string old_root_dir = new_root + put_old;
    if (!ensure_directory(old_root_dir)) return false;
    if (pivot_root(new_root.c_str(), old_root_dir.c_str()) != 0) {
        log_error("pivot_root syscall failed"); return false;
    }
    if (chdir("/") != 0) {
        log_error("Failed to chdir to new root"); return false;
    }
    if (umount2(put_old.c_str(), MNT_DETACH) != 0) {
        log_error("Failed to unmount old root"); return false;
    }
    if (rmdir(put_old.c_str()) != 0) {
        log_error("Failed to remove old root directory"); return false;
    }
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
    std::vector<sock_filter> filter;

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
// SecurityManager Implementation (Orchestrator)
// ============================================================================

bool SecurityManager::apply_parent_mappings(const SecurityConfig& config, pid_t child_pid) {
    if (config.use_user_namespace) {
        if (!UserSecurity::setup_setgroups(child_pid, false)) return false;
        if (!UserSecurity::write_uid_map(child_pid, config.uid_mappings)) return false;
        if (!UserSecurity::write_gid_map(child_pid, config.gid_mappings)) return false;
    }
    return true;
}

// Replace the /dev setup section in apply_child_security with this improved version:

bool SecurityManager::apply_child_security(const SecurityConfig& config, const std::string& hostname, const std::string& rootfs) {
    if (sethostname(hostname.c_str(), hostname.length()) != 0) {
        log_error("sethostname failed"); return false;
    }
    // Security.cpp (early in apply_child_security, before pivot_root)
// Make all mounts private to stop propagation to host
if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
    log_error("Failed to set mount propagation to private");
    return false;
}

    if (!FilesystemSecurity::setup_pivot_root(rootfs, "/.old_root")) return false;

    // Mount essential virtual filesystems
    FilesystemSecurity::ensure_directory("/proc");
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, nullptr) != 0) {
        log_error("mount /proc"); return false;
    }

    FilesystemSecurity::ensure_directory("/sys");
    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY, nullptr) != 0) {
        log_error("mount /sys"); return false;
    }
// Security.cpp (apply_child_security)
if (mount("proc", "/proc", "proc", MS_NOSUID|MS_NODEV|MS_NOEXEC, "hidepid=2") != 0) { log_error("mount proc"); return false; }
// Optionally: bind-mount tmpfs over /proc/sys* to block sysctl writes

    // Setup /dev - improved approach
    FilesystemSecurity::ensure_directory("/dev");

    // Try devtmpfs first, fall back to tmpfs if not available
    if (mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755") != 0) {
        std::cerr << "[Security] devtmpfs failed, trying tmpfs for /dev" << std::endl;
        if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755") != 0) {
            log_error("mount /dev (both devtmpfs and tmpfs failed)");
            return false;
        }

        // Create essential device nodes manually if using tmpfs
        mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
        mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));
        mknod("/dev/full", S_IFCHR | 0666, makedev(1, 7));
        mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
        mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
        mknod("/dev/tty", S_IFCHR | 0666, makedev(5, 0));
    }

    // Setup devpts with proper options
    FilesystemSecurity::ensure_directory("/dev/pts");
    const char* devpts_opts = "newinstance,ptmxmode=0666,mode=0620,gid=5";
    if (mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, devpts_opts) != 0) {
        log_error("mount /dev/pts");
        return false;
    }

    // Create /dev/ptmx symlink - remove existing first
    unlink("/dev/ptmx");
    if (symlink("pts/ptmx", "/dev/ptmx") != 0) {
        log_error("symlink /dev/ptmx");
        // Non-fatal - continue
    }

    // Create standard fd symlinks
    unlink("/dev/fd");
    unlink("/dev/stdin");
    unlink("/dev/stdout");
    unlink("/dev/stderr");

    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");

    // Create /dev/shm for shared memory
    FilesystemSecurity::ensure_directory("/dev/shm");
    if (mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") != 0) {
        std::cerr << "[Security] Warning: Failed to mount /dev/shm" << std::endl;
        // Non-fatal
    }
    // ========================================================================
    // PHASE 1: All filesystem operations (requires CAP_SYS_ADMIN)
    // ========================================================================

    // Bind mounts first
    for (const auto& mount : config.bind_mounts) {
        if (!FilesystemSecurity::create_bind_mount(mount.first, mount.second, false)) {
            return false;
        }
    }

    // Setup tmpfs at /tmp
    if (config.setup_tmpfs) {
        if (!FilesystemSecurity::setup_tmpfs("/tmp", config.tmpfs_size_mb)) {
            return false;
        }
    }

    // Make rootfs read-only if requested
    if (config.readonly_rootfs) {
        if (!FilesystemSecurity::mount_readonly("/")) {
            return false;
        }
    }

    // ========================================================================
    // PHASE 2: Drop privileges (no more mounts after this point)
    // ========================================================================

    // NOW drop capabilities after all mounts are complete
    if (config.drop_capabilities) {
        if (!CapabilityManager::drop_capabilities(config.keep_capabilities)) {
            return false;
        }
    }

    // Drop to unprivileged user
    if (config.use_user_namespace) {
        if (!UserSecurity::drop_to_user(config.container_uid, config.container_gid)) {
            return false;
        }
    }

    // ========================================================================
    // PHASE 3: Apply seccomp (most restrictive, must be last)
    // ========================================================================

    if (config.use_seccomp) {
        if (config.seccomp_profile == "default") {
            if (!SeccompFilter::apply_default_profile()) {
                return false;
            }
        }
    }


    return true;
}

} // namespace Security
