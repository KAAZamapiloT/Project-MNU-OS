#include "Container.h"
#include "FilesystemManager.h"
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sched.h> // For clone()

// Stack size for the child process
const int STACK_SIZE = 1024 * 1024; // 1 MB

// --- FIX: This is the updated constructor that accepts a Config object ---
Container::Container(const Config& config)
    : config_(config),
      stack_memory_(new char[STACK_SIZE]),
      cgroup_manager_(config) {} // Initialize the CgroupManager here

int Container::child_function(void* arg) {
    ChildArgs* args = static_cast<ChildArgs*>(arg);
    const Config* config = args->config;

    std::cout << "[Child] Process started with PID: " << getpid() << std::endl;

    // Set hostname
    if (sethostname(config->hostname.c_str(), config->hostname.length()) != 0) {
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
    // --- Parent Process Logic ---
    
    // 1. Setup Cgroups before cloning
    if (!cgroup_manager_.setup()) {
        std::cerr << "Error: Cgroup setup failed." << std::endl;
        return 1;
    }

    ChildArgs child_args;
    child_args.config = &config_;

    void* stack_top = stack_memory_.get() + STACK_SIZE;

    // 2. Create the child process with clone()
    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;
    pid_t child_pid = clone(child_function, stack_top, flags, &child_args);

    if (child_pid == -1) {
        std::cerr << "Error: clone failed: " << strerror(errno) << std::endl;
        cgroup_manager_.teardown(); // Clean up cgroups on failure
        return 1;
    }
    
    // 3. Apply Cgroup limits to the new child process
    if (!cgroup_manager_.apply(child_pid)) {
        std::cerr << "Error: Failed to apply cgroup to child process." << std::endl;
    }

    // 4. Wait for the child process to exit
    int status = 0;
    waitpid(child_pid, &status, 0);

    // 5. Teardown Cgroups
    cgroup_manager_.teardown();

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

