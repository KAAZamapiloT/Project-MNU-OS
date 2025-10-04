#ifndef CONTAINER_H
#define CONTAINER_H

#include "Config.h"
#include "CgroupManager.h"
#include <string>
#include <vector>
#include <memory>

class Container {
public:
    explicit Container(const Config& config);
    int run(); // Runs in foreground
    pid_t start(); // Runs in background (detached)

private:
    // This struct now includes a flag to control terminal detachment.
    struct ChildArgs {
        const Config* config;
        bool detached;
    };

    static int child_function(void* arg);

    Config config_;
    std::unique_ptr<char[]> stack_memory_;
    CgroupManager cgroup_manager_;
};

#endif // CONTAINER_H

