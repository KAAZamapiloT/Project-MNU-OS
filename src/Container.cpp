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

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
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

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
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

bool setup_simple_chroot(const Config* config) {
    if (chroot(config->rootfs_path.c_str()) != 0) {
        perror("chroot failed");
        return false;
    }
    if (chdir("/") != 0) {
        perror("chdir to new root failed");
        return false;
    }

    mkdir("/proc", 0755);
    if (mount("proc", "/proc", "proc", 0, nullptr) != 0) {
        perror("mount /proc failed");
    }

    mkdir("/dev", 0755);
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755") != 0) {
         perror("mount /dev failed");
    }

    mkdir("/dev/pts", 0755);
    if (mount("devpts", "/dev/pts", "devpts", 0, nullptr) != 0) {
        perror("mount /dev/pts failed");
    }

    return true;
}

// Remove send_fd and recv_fd - we won't use them

int Container::child_function(void* arg) {
    ChildArgs* args = static_cast<ChildArgs*>(arg);
    const Config* config = args->config;

    // 1. Wait for parent signal
    char buf;
    close(args->ctrl_socks[0]);
    read(args->ctrl_socks[1], &buf, 1);
    close(args->ctrl_socks[1]);  // Close immediately after reading

    // 2. Set hostname
    if (sethostname(config->hostname.c_str(), config->hostname.length()) != 0) {
        perror("sethostname failed");
        return 1;
    }

    // 3. Apply security
    if (config->security.use_pivot_root) {
        if (!Security::SecurityManager::apply_child_security(
                config->security, config->hostname, config->rootfs_path)) {
            std::cerr << "Error: Advanced security setup failed." << std::endl;
            return 1;
        }
    } else {
        if (!setup_simple_chroot(config)) {
            std::cerr << "Error: Simple chroot setup failed." << std::endl;
            return 1;
        }
    }

    // 4. Setup stdio - NO PTY for now, just inherit from parent
    if (args->detached) {
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null != -1) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
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

    execvp(c_args[0], c_args.data());
    perror("execvp failed");
    return 1;
}

pid_t Container::create_container_process(bool detached) {
    int ctrl_socks[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ctrl_socks) != 0) {
        perror("socketpair failed");
        return -1;
    }

    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC | SIGCHLD;
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
    close(ctrl_socks[1]);  // Close immediately after signaling

    return child_pid;
}


int Container::run() {
    if (!cgroup_manager_.setup()) return 1;

    pid_t child_pid = create_container_process(false);
    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }

    cgroup_manager_.apply(child_pid);

    int status = 0;
    waitpid(child_pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        std::cerr << "[Main] Container process exited with error code: "
                  << WEXITSTATUS(status) << std::endl;
    }

    cgroup_manager_.teardown();
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

pid_t Container::start() {
    if (!cgroup_manager_.setup()) return -1;

    pid_t child_pid = create_container_process(true);
    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }

    cgroup_manager_.apply(child_pid);
    std::cout << "[Main] Container '" << config_.hostname
              << "' started with PID: " << child_pid << std::endl;
    return child_pid;
}
