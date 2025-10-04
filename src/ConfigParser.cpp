#include "ConfigParser.h"
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool ConfigParser::parse(const std::string& filepath, Config& out_config) {
    std::ifstream config_file(filepath);
    if (!config_file.is_open()) {
        std::cerr << "Error: Could not open config file: " << filepath << std::endl;
        return false;
    }

    try {
        json data = json::parse(config_file);

        out_config.hostname = data.value("hostname", "mun-os-container");
        out_config.rootfs_path = data.at("rootfs_path"); // at() throws if not found
        out_config.command = data.at("command");

        if (data.contains("args")) {
            out_config.args = data["args"].get<std::vector<std::string>>();
        }

        if (data.contains("resources")) {
            out_config.memory_limit_mb = data["resources"].value("memory_limit_mb", 0);
            out_config.process_limit = data["resources"].value("process_limit", 0);
        }

    } catch (json::exception& e) {
        std::cerr << "Error: Failed to parse JSON config file: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool ConfigParser::validate(const Config& config) {
    if (config.rootfs_path.empty()) {
        std::cerr << "Validation Error: 'rootfs_path' is a required field." << std::endl;
        return false;
    }
    if (config.command.empty()) {
        std::cerr << "Validation Error: 'command' is a required field." << std::endl;
        return false;
    }
    // Add more validation as needed (e.g., check if rootfs_path exists)
    return true;
}
