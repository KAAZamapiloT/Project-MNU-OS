#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include "Security.h"  // Include the nested security configuration

struct Config {
    // Basic Configuration
    // FIX: Added a default initializer for hostname.
    std::string hostname = "mun-os-container";
    std::string rootfs_path;
    std::string command;
    std::vector<std::string> args;
    
    // Resource Limits
    int memory_limit_mb = 0;
    int process_limit = 0;
    
    // Encapsulated Security Configuration
    Security::SecurityConfig security;
};

#endif // CONFIG_H

