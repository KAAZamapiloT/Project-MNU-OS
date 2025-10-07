#include "ConfigParser.h"
#include <fstream>
#include <iostream>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool ConfigParser::parse_json(const std::string& filepath, Config& out_config) {
    std::ifstream config_file(filepath);
    if (!config_file.is_open()) {
        std::cerr << "Error: Could not open config file: " << filepath << std::endl;
        return false;
    }

    try {
        json data = json::parse(config_file);

        // Parse hostname (optional, has default)
        out_config.hostname = data.value("hostname", "mun-os-container");
        
        // Parse rootfs_path (required)
        out_config.rootfs_path = data.at("rootfs_path");
        
        // Parse command (required)
        out_config.command = data.at("command");

        // Parse args (optional)
        if (data.contains("args")) {
            out_config.args = data["args"].get<std::vector<std::string>>();
        } else {
            out_config.args.clear();
        }

        // Parse resource limits (support both nested and flat formats)
        // Format 1: Nested under "resources"
        if (data.contains("resources")) {
            out_config.memory_limit_mb = data["resources"].value("memory_limit_mb", 0);
            out_config.process_limit = data["resources"].value("process_limit", 0);
        } 
        // Format 2: Flat at root level (for backwards compatibility)
        else {
            out_config.memory_limit_mb = data.value("memory_limit_mb", 0);
            out_config.process_limit = data.value("process_limit", 0);
        }

        std::cout << "[ConfigParser] Successfully parsed config:" << std::endl;
        std::cout << "  Hostname: " << out_config.hostname << std::endl;
        std::cout << "  Rootfs: " << out_config.rootfs_path << std::endl;
        std::cout << "  Command: " << out_config.command << std::endl;
        std::cout << "  Memory Limit: " << out_config.memory_limit_mb << " MB" << std::endl;
        std::cout << "  Process Limit: " << out_config.process_limit << std::endl;

    } catch (json::exception& e) {
        std::cerr << "Error: Failed to parse JSON config file: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool ConfigParser::validate(const Config& config) {
    bool valid = true;

    if (config.rootfs_path.empty()) {
        std::cerr << "Validation Error: 'rootfs_path' is a required field." << std::endl;
        valid = false;
    }
    
    if (config.command.empty()) {
        std::cerr << "Validation Error: 'command' is a required field." << std::endl;
        valid = false;
    }

    // Check if rootfs exists
    if (!config.rootfs_path.empty()) {
        std::ifstream test(config.rootfs_path + "/bin/sh");
        if (!test.good()) {
            std::cerr << "Warning: rootfs_path may not be valid (missing /bin/sh)" << std::endl;
        }
    }

    if (config.memory_limit_mb < 0) {
        std::cerr << "Validation Error: memory_limit_mb cannot be negative" << std::endl;
        valid = false;
    }

    if (config.process_limit < 0) {
        std::cerr << "Validation Error: process_limit cannot be negative" << std::endl;
        valid = false;
    }

    return valid;
}