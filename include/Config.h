#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include "Security.h" // Include the new security definitions

// This struct holds all configuration for a single container.
struct Config {
    // Basic container settings
    std::string hostname = "mun-os-container";
    std::string rootfs_path;
    std::string command;
    std::vector<std::string> args;
    
    // Resource limits (cgroups)
    int memory_limit_mb = 0;
    int process_limit = 0;
    
    // All security-related configurations are now neatly grouped here.
    Security::SecurityConfig security;
};

#endif // CONFIG_H

