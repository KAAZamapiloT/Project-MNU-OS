#include "Container.h"
#include "FilesystemManager.h" // Include the new FilesystemManager
#include <iostream>
#include <memory>       // For std::unique_ptr
#include <sys/wait.h>   // For waitpid()
#include <unistd.h>     // For execvp(), sethostname()
#include <cerrno>       // For errno
#include <cstring>      // For strerror()

// A reasonable stack size for the child process
const int STACK_SIZE = 1 * 1024 * 1024; // 1 MB

Container::Container(
    const std::string& hostname,
    const std::string& rootfs_path,
    const std::string& command,
    const std::vector<std::string>& args)
    : hostname_(hostname), rootfs_path_(rootfs_path), command_(command), args_(args) {}

int Container::run() {
    // Use a smart pointer for the stack for automatic memory management (RAII).
    auto stack = std::make_unique<char[]>(STACK_SIZE);
    void* stack_top = stack.get() + STACK_SIZE;

    // Flags for clone(): new PID, UTS, and Mount namespaces.
    // SIGCHLD ensures the parent is notified when the child exits.
    int clone_flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;

    // Create the child process using clone().
    pid_t pid = clone(child_function, stack_top, clone_flags, this);

    if (pid == -1) {
        std::cerr << "Error: clone() failed: " << strerror(errno) << std::endl;
        return -1;
    }

    // --- Parent Process ---
    int status;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int Container::child_function(void* arg) {
    // --- Child Process ---
    Container* self = static_cast<Container*>(arg);

    // 1. Set the hostname
    if (sethostname(self->hostname_.c_str(), self->hostname_.length()) != 0) {
        std::cerr << "Error: sethostname() failed: " << strerror(errno) << std::endl;
        _exit(1);
    }

    // 2. Set up the root filesystem
    FilesystemManager fs_manager(self->rootfs_path_);
    if (!fs_manager.setup()) {
        std::cerr << "Error: Failed to set up container filesystem." << std::endl;
        _exit(1);
    }
    
    // 3. Prepare arguments for execvp
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(self->command_.c_str()));
    for (const auto& arg_str : self->args_) {
        c_args.push_back(const_cast<char*>(arg_str.c_str()));
    }
    c_args.push_back(nullptr);

    // 4. Execute the user's command
    execvp(self->command_.c_str(), c_args.data());

    // If execvp returns, an error occurred.
    std::cerr << "Error: execvp failed: " << strerror(errno) << std::endl;
    _exit(1);
}

