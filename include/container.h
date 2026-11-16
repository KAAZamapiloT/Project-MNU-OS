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
    // This struct passes essential info to the child process.
    // Container.h
    struct ChildArgs {
        const Config* config;    // ✅ CORRECT - pointer
        bool detached;
        int ctrl_socks[2];
        int slave_fd;
    };




    static int child_function(void* arg);
    pid_t create_container_process(bool detached, int& out_ctrl_fd, int& out_master_fd);

    Config config_;
    std::unique_ptr<char[]> stack_memory_;
    CgroupManager cgroup_manager_;
};

#endif // CONTAINER_H
