#include "Container.h"
#include "Security.h"
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sched.h>
#include <fcntl.h>

const int STACK_SIZE = 1024 * 1024;

Container::Container(const Config& config)
    : config_(config),
      stack_memory_(new char[STACK_SIZE]),
      cgroup_manager_(config) {}

// The entry point for the new container process.
int Container::child_function(void* arg) {
    ChildArgs* args = static_cast<ChildArgs*>(arg);
    const Config* config = args->config;

    // 1. Wait for the parent to finish setting up user/group ID mappings.
    char buf;
    close(args->pipe_fd + 1); // Close write end of the pipe
    read(args->pipe_fd, &buf, 1); // This blocks until the parent writes
    close(args->pipe_fd);

    // 2. Set the container's hostname.
    if (sethostname(config->hostname.c_str(), config->hostname.length()) != 0) {
        perror("sethostname failed");
        return 1;
    }

    // 3. Apply all security features (replaces the old FilesystemManager).
    if (!Security::SecurityManager::apply_child_security(config->security, config->rootfs_path)) {
        std::cerr << "Error: Child security setup failed." << std::endl;
        return 1;
    }

    // 4. Detach stdio if running in the background.
    if (args->detached) {
        int dev_null = open("/dev/null", O_RDWR);
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }

    // 5. Prepare arguments and execute the user's command.
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(config->command.c_str()));
    for (const auto& a : config->args) {
        c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(c_args[0], c_args.data());

    // Should not be reached
    perror("execvp failed");
    return 1;
}

// Sets up and creates the new container process.
pid_t Container::create_container_process(bool detached) {
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return -1;
    }

    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;
    if (config_.security.use_user_namespace) {
        flags |= CLONE_NEWUSER;
    }

    ChildArgs child_args{&config_, detached, pipe_fds[0]};
    void* stack_top = stack_memory_.get() + STACK_SIZE;
    
    pid_t child_pid = clone(child_function, stack_top, flags, &child_args);

    if (child_pid == -1) {
        perror("clone failed");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return -1;
    }

    // --- PARENT PROCESS LOGIC ---
    close(pipe_fds[0]); // Parent only needs the write end

    // Write UID/GID maps for the new process from the parent side.
    if (!Security::SecurityManager::apply_parent_mappings(config_.security, child_pid)) {
        kill(child_pid, SIGKILL); // Kill the child if mapping fails
        return -1;
    }

    // Signal the child that it can now proceed.
    write(pipe_fds[1], "1", 1);
    close(pipe_fds[1]);

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
    waitpid(child_pid, nullptr, 0); // Wait for foreground process
    cgroup_manager_.teardown();
    return 0;
}

pid_t Container::start() {
    if (!cgroup_manager_.setup()) return -1;

    pid_t child_pid = create_container_process(true);
    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }
    
    cgroup_manager_.apply(child_pid);
    std::cout << "[Main] Container '" << config_.hostname << "' started with PID: " << child_pid << std::endl;
    return child_pid;
}

