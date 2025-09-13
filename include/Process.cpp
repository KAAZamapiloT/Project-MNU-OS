#include "Process.h"
#include <iostream>
#include <vector>
#include <memory>       // For std::unique_ptr
#include <sys/wait.h>   // For waitpid()
#include <cerrno>       // For errno
#include <cstring>      // For strerror()

// Define a stack size for the child process
const int STACK_SIZE = 1 * 1024 * 1024; // 1 MB

Process::Process(const std::string& command, const std::vector<std::string>& args)
    : command_(command), args_(args) {}

int Process::run(int flags) {
    // 1. ALLOCATE THE STACK for the child process.
    // We use a smart pointer for automatic memory management to prevent leaks.
    auto stack = std::make_unique<char[]>(STACK_SIZE);
    // Stacks on most architectures grow downwards, so we point to the top of the memory block.
    void* stack_top = stack.get() + STACK_SIZE;

    // 2. CALL clone() to create the child process.
    // We pass `this` as the argument so the static child_entry_point function
    // can access the command_ and args_ member variables.
    pid_t pid = clone(child_entry_point, stack_top, flags, this);

    if (pid == -1) {
        std::cerr << "Error: clone() failed: " << strerror(errno) << std::endl;
        return -1;
    }

    // 3. PARENT PROCESS: Wait for the child to exit.
    int status;
    waitpid(pid, &status, 0);

    // The unique_ptr for the stack is automatically freed when this function returns.

    // Return the exit code of the child process
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int Process::child_entry_point(void* arg) {
    // This code is executed by the child process.
    
    // Cast the void* argument back to a Process* to access member variables.
    Process* self = static_cast<Process*>(arg);

    // Prepare the arguments for execvp. It needs a C-style array of char pointers,
    // terminated by a nullptr.
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(self->command_.c_str()));

    for (const auto& arg_str : self->args_) {
        c_args.push_back(const_cast<char*>(arg_str.c_str()));
    }
    c_args.push_back(nullptr);

    // Execute the user's command. If this is successful, this child process
    // is completely replaced by the new program.
    execvp(self->command_.c_str(), c_args.data());

    // If execvp returns, it means an error occurred.
    // We use _exit() in a child from clone() to avoid running atexit handlers
    // that might belong to the parent.
    std::cerr << "Error: execvp failed: " << strerror(errno) << std::endl;
    _exit(1);
}

