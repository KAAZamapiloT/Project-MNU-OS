#include "Container.h"
#include "FilesystemManager.h"
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sched.h>

const int STACK_SIZE = 1024 * 1024;

Container::Container(const Config& config)
    : config_(config),
      stack_memory_(new char[STACK_SIZE]),
      cgroup_manager_(config) {}

// child_function remains the same as before...
int Container::child_function(void* arg) {
    // ... same implementation ...
}

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

