#include "Container.h"
#include "FilesystemManager.h"
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sched.h> // For clone()
#include <fcntl.h> // For open()

const int STACK_SIZE = 1024 * 1024;

Container::Container(const Config& config)
    : config_(config),
      stack_memory_(new char[STACK_SIZE]),
      cgroup_manager_(config) {}

int Container::child_function(void* arg) {
    ChildArgs* args = static_cast<ChildArgs*>(arg);
    const Config* config = args->config;

    // --- FIX: Only detach from the terminal if running in the background ---
    if (args->detached) {
        // Redirect standard input, output, and error to /dev/null
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null == -1) return 1;
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }
    // For 'run', stdin/out/err remain connected to the user's terminal.

    // Set hostname
    if (sethostname(config->hostname.c_str(), config->hostname.length()) != 0) {
        // Errors will now correctly print to the terminal in 'run' mode.
        std::cerr << "Error: sethostname failed: " << strerror(errno) << std::endl;
        return 1;
    }

    // Setup filesystem
    FilesystemManager fs_manager(config->rootfs_path);
    if (!fs_manager.setup()) {
        std::cerr << "Error: Filesystem setup failed." << std::endl;
        return 1;
    }

    // Prepare arguments for execvp
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(config->command.c_str()));
    for (const auto& a : config->args) {
        c_args.push_back(const_cast<char*>(a.c_str()));
    }
    c_args.push_back(nullptr);

    // Execute the user's command
    execvp(c_args[0], c_args.data());

    // If execvp returns, it must have failed
    std::cerr << "Error: execvp failed: " << strerror(errno) << std::endl;
    return 1;
}

int Container::run() {
    if (!cgroup_manager_.setup()) return 1;

    // Set detached to false for foreground mode
    ChildArgs child_args{&config_, false};
    void* stack_top = stack_memory_.get() + STACK_SIZE;
    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;
    
    pid_t child_pid = clone(child_function, stack_top, flags, &child_args);
    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }
    
    if (!cgroup_manager_.apply(child_pid)) { /* handle error */ }

    int status = 0;
    waitpid(child_pid, &status, 0);
    
    cgroup_manager_.teardown();
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

pid_t Container::start() {
    if (!cgroup_manager_.setup()) return -1;

    // Set detached to true for background mode
    ChildArgs child_args{&config_, true};
    void* stack_top = stack_memory_.get() + STACK_SIZE;
    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;
    
    pid_t child_pid = clone(child_function, stack_top, flags, &child_args);

    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }
    
    if (!cgroup_manager_.apply(child_pid)) { /* handle error */ }

    std::cout << "[Main] Container started in background with PID: " << child_pid << std::endl;
    return child_pid;
}

