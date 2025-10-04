#ifndef CONFIG_H
#define CONFIG_H

#include <string>  // <-- FIX: Added this include
#include <vector>  // <-- FIX: Added this include

/**
 * @struct Config
 * @brief A simple data structure to hold all container configuration settings.
 */
struct Config {
    std::string hostname = "mun-os-container"; // A default hostname
    std::string rootfs_path;
    std::string command;
    std::vector<std::string> args;

    // Resource limits (0 means no limit)
    int memory_limit_mb = 0;
    int process_limit = 0;
};

#endif // CONFIG_H

