#ifndef CONTAINER_H
#define CONTAINER_H

#include <string>
#include <vector>

// Define _GNU_SOURCE to get the declaration of clone() from <sched.h>
#define _GNU_SOURCE
#include <sched.h>

/**
 * @class Container
 * @brief Manages the creation and execution of an isolated process.
 */
class Container {
public:
    /**
     * @brief Constructs a Container object.
     * @param hostname The desired hostname for the container.
     * @param rootfs_path The path to the root filesystem for the container.
     * @param command The command to run inside the container.
     * @param args A vector of arguments for the command.
     */
    Container(
        const std::string& hostname,
        const std::string& rootfs_path,
        const std::string& command,
        const std::vector<std::string>& args
    );

    /**
     * @brief Runs the container.
     * @return The exit status of the containerized process, or -1 on failure.
     */
    int run();

private:
    /**
     * @brief The static entry point for the child process created by clone().
     * @param arg A pointer to the Container object (`this`).
     */
    static int child_function(void* arg);

    // Member variables to hold the container's configuration
    std::string hostname_;
    std::string rootfs_path_;
    std::string command_;
    std::vector<std::string> args_;
};

#endif // CONTAINER_H

