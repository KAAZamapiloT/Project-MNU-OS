#ifndef CONTAINER_H
#define CONTAINER_H

#include "Config.h"
#include "CgroupManager.h"
#include <string>
#include <vector>
#include <memory>
#include <unistd.h>

class Container {
public:
    explicit Container(const Config& config);

    int run();
    pid_t start();

private:
    // Arguments passed to the new process, including a pipe for synchronization.
    struct ChildArgs {
        const Config* config;
        bool detached;
        int pipe_fd; // Used to signal child process after parent setup is complete
    };

    static int child_function(void* arg);
    pid_t create_container_process(bool detached);

    Config config_;
    std::unique_ptr<char[]> stack_memory_;
    CgroupManager cgroup_manager_;
};

#endif // CONTAINER_H

