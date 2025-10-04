#include "Container.h"
#include "FilesystemManager.h"
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sched.h> // For clone()
#include <fcntl.h> // For open() and file flags

const int STACK_SIZE = 1024 * 1024; // 1 MB

Container::Container(const Config& config)
    : config_(config),
      stack_memory_(new char[STACK_SIZE]),
      cgroup_manager_(config) {}

int Container::child_function(void* arg) {
    ChildArgs* args = static_cast<ChildArgs*>(arg);
    const Config* config = args->config;

    // This first std::cout will now be redirected to /dev/null
    std::cout << "[Child] Process started with PID: " << getpid() << std::endl;

    // --- NEW: DETACH FROM TERMINAL FOR TRUE DAEMONIZATION ---
    // Redirect standard input, output, and error to /dev/null
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1) {
        // Cannot open /dev/null, something is very wrong.
        // We can't even print an error, so we just exit.
        return 1;
    }
    dup2(dev_null, STDIN_FILENO);  // stdin
    dup2(dev_null, STDOUT_FILENO); // stdout
    dup2(dev_null, STDERR_FILENO); // stderr
    close(dev_null);
    // --- END NEW SECTION ---

    // Set hostname
    if (sethostname(config->hostname.c_str(), config->hostname.length()) != 0) {
        // Errors from here on will be sent to /dev/null and won't appear on your terminal.
        // In a real application, you would log these to a file.
        return 1;
    }

    // Setup filesystem
    FilesystemManager fs_manager(config->rootfs_path);
    if (!fs_manager.setup()) {
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
    return 1;
}

// run() and start() methods remain the same...
int Container::run() {
    // This is now the "attached" or "foreground" mode
    if (!cgroup_manager_.setup()) return 1;

    ChildArgs child_args{&config_};
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
    // This is the new "detached" or "background" mode
    if (!cgroup_manager_.setup()) return -1;

    ChildArgs child_args{&config_};
    void* stack_top = stack_memory_.get() + STACK_SIZE;
    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;
    
    pid_t child_pid = clone(child_function, stack_top, flags, &child_args);

    if (child_pid == -1) {
        cgroup_manager_.teardown();
        return -1;
    }
    
    if (!cgroup_manager_.apply(child_pid)) { /* handle error */ }

    std::cout << "[Main] Container started in background with PID: " << child_pid << std::endl;
    // The parent does NOT wait. It returns the child's PID immediately.
    return child_pid;
}

