#ifndef CONTAINER_H
#define CONTAINER_H

#include <string>
#include <vector>
#include <memory>         // For std::unique_ptr
#include "Config.h"       // Now uses the Config struct
#include "CgroupManager.h" // Include the CgroupManager

// Note: _GNU_SOURCE will be moved to CMakeLists.txt to avoid redefinition warnings

class Container {
public:
    /**
     * @brief Constructs a Container object from a configuration struct.
     * @param config The configuration object containing all settings.
     */
    explicit Container(const Config& config);

    /**
     * @brief Runs the container.
     * @return The exit status of the containerized process, or -1 on failure.
     */
    int run();

private:
    // A private struct to pass arguments to the clone()'d child process
    struct ChildArgs {
        const Config* config;
    };

    /**
     * @brief The static entry point for the child process created by clone().
     * @param arg A pointer to a ChildArgs struct.
     */
    static int child_function(void* arg);

    Config config_;
    std::unique_ptr<char[]> stack_memory_;
    CgroupManager cgroup_manager_;
};

#endif // CONTAINER_H

