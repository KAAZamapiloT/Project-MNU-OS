#include "Container.h"
#include "Security.h"
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sched.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <climits>  // ✅ FIX: Added for PATH_MAX
#include "NetworkManager.h"

// Helper functions for fd passing (not used currently, but kept for future)
static bool send_fd(int socket, int fd_to_send) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[1] = {'X'};

    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    char control_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    if (sendmsg(socket, &msg, 0) < 0) {
        perror("sendmsg");
        return false;
    }
    return true;
}

static int recv_fd(int socket) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[1];

    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    char control_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    if (recvmsg(socket, &msg, 0) < 0) {
        perror("recvmsg");
        return -1;
    }

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr || cmsg->cmsg_type != SCM_RIGHTS) {
        std::cerr << "Failed to receive file descriptor via SCM_RIGHTS" << std::endl;
        return -1;
    }

    int received_fd;
    memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    return received_fd;
}

const int STACK_SIZE = 1024 * 1024;

Container::Container(const Config& config)
    : config_(config),
      stack_memory_(new char[STACK_SIZE]),
      cgroup_manager_(config) {}

// Simple chroot-based setup (fallback when pivot_root is disabled)
bool setup_simple_chroot(const Config* config) {
    if (chroot(config->rootfs_path.c_str()) != 0) {
        perror("chroot failed");
        return false;
    }
    if (chdir("/") != 0) {
        perror("chdir to new root failed");
        return false;
    }

    mkdir("proc", 0755);
    if (mount("proc", "proc", "proc", 0, nullptr) != 0) {
        perror("mount proc failed");
    }

    mkdir("dev", 0755);
    if (mount("tmpfs", "dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755") != 0) {
        perror("mount dev failed");
    }

    mkdir("dev/pts", 0755);
    if (mount("devpts", "dev/pts", "devpts", 0, nullptr) != 0) {
        perror("mount devpts failed");
    }

    return true;
}

// Child process entry point
int Container::child_function(void* arg) {
    ChildArgs* args = static_cast<ChildArgs*>(arg);
    const Config* config = args->config;

    // 1. Wait for parent signal
    char buf;
    close(args->ctrl_socks[0]);  // ✅ FIX: Changed from ctrl_socks to ctrl_socks
    read(args->ctrl_socks[1], &buf, 1);  // ✅ FIX: Changed from ctrl_socks to ctrl_socks
    close(args->ctrl_socks[1]);  // ✅ FIX: Changed from ctrl_socks to ctrl_socks

    // 2. Set hostname
    std::cout << "[Child] Setting hostname to " << config->hostname << std::endl;
    if (sethostname(config->hostname.c_str(), config->hostname.length()) != 0) {
        perror("[Child] sethostname failed");
        return 1;
    }

    // 3. Apply security with detailed logging
    std::cout << "[Child] Applying security configuration..." << std::endl;
    std::cout << "[Child] - use_pivot_root: " << config->security.use_pivot_root << std::endl;
    std::cout << "[Child] - rootfs_path: " << config->rootfs_path << std::endl;

    if (config->security.use_pivot_root) {
        std::cout << "[Child] Starting advanced security setup (pivot_root)..." << std::endl;
        if (!Security::SecurityManager::apply_child_security(
                config->security, config->hostname, config->rootfs_path)) {
            std::cerr << "[Child] ERROR: Advanced security setup failed" << std::endl;
            std::cerr << "[Child] Check dmesg for kernel-level errors" << std::endl;
            return 1;
        }
        std::cout << "[Child] Advanced security setup complete" << std::endl;
    } else {
        std::cout << "[Child] Starting simple chroot setup..." << std::endl;
        if (!setup_simple_chroot(config)) {
            std::cerr << "[Child] ERROR: Simple chroot setup failed" << std::endl;
            return 1;
        }
        std::cout << "[Child] Simple chroot setup complete" << std::endl;
    }

    std::cout << "[Child] Preparing to execute: " << config->command << std::endl;

    // 4. Setup stdio - NO PTY for now, just inherit from parent
    if (args->detached) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
    }
    // For interactive mode, just use inherited stdin/stdout/stderr

    // 5. Execute command
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(config->command.c_str()));
    for (const auto& a : config->args) {
        c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    std::cout << "[Child] execvp: " << c_args[0] << std::endl;
    execvp(c_args[0], c_args.data());

    // Only reached if execvp fails
    perror("[Child] execvp failed");
    std::cerr << "[Child] Command: " << config->command << std::endl;
    std::cerr << "[Child] CWD: ";
    char cwd[PATH_MAX];  // ✅ FIX: Now PATH_MAX is defined
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cerr << cwd << std::endl;
    }
    std::cerr << "[Child] File exists check: ";
    if (access(config->command.c_str(), X_OK) == 0) {
        std::cerr << "YES (executable)" << std::endl;
    } else {
        std::cerr << "NO (" << strerror(errno) << ")" << std::endl;
    }

    return 1;
}

pid_t Container::create_container_process(bool detached) {
    int ctrl_socks[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ctrl_socks) != 0) {
        perror("socketpair failed");
        return -1;
    }

    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC | CLONE_NEWNET | SIGCHLD;
    if (config_.security.use_user_namespace) {
        flags |= CLONE_NEWUSER;
    }

    ChildArgs child_args{&config_, detached, {ctrl_socks[0], ctrl_socks[1]}};
    void* stack_top = stack_memory_.get() + STACK_SIZE;

    pid_t child_pid = clone(child_function, stack_top, flags, &child_args);

    if (child_pid == -1) {
        perror("clone failed");
        close(ctrl_socks[0]);
        close(ctrl_socks[1]);
        return -1;
    }

    close(ctrl_socks[0]);

    if (!Security::SecurityManager::apply_parent_mappings(config_.security, child_pid)) {
        kill(child_pid, SIGKILL);
        close(ctrl_socks[1]);
        return -1;
    }

    // Signal child to proceed
    write(ctrl_socks[1], "1", 1);
    close(ctrl_socks[1]);

    return child_pid;
}

int Container::run() {
    if (!cgroup_manager_.setup()) {
        return 1;
    }

    pid_t child_pid = create_container_process(false);
    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }

    NetworkManager net_manager(config_.network);
    if (!net_manager.setup_container_network(child_pid, config_.hostname)) {
        std::cerr << "[Main] Failed to setup network, killing container" << std::endl;
        kill(child_pid, SIGKILL);
        cgroup_manager_.teardown();
        return -1;
    }

    cgroup_manager_.apply(child_pid);  // ✅ FIX: Changed from apply_to_proces to apply

    // Add delay to ensure network is fully ready
    usleep(200000);

    int status = 0;
    waitpid(child_pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        std::cerr << "[Main] Container process exited with error code "
                  << WEXITSTATUS(status) << std::endl;
    }

    cgroup_manager_.teardown();
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

pid_t Container::start() {
    if (!cgroup_manager_.setup()) {
        return -1;
    }

    pid_t child_pid = create_container_process(true);
    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }

    cgroup_manager_.apply(child_pid);  // ✅ FIX: Changed from apply_to_proces to apply

    std::cout << "[Main] Container " << config_.hostname
              << " started with PID " << child_pid << std::endl;

    return child_pid;
}
