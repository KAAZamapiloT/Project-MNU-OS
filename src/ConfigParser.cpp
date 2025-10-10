#include "ConfigParser.h"
#include <fstream>
#include <iostream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool ConfigParser::parse_json(const std::string& filepath, Config& out_config) {
    std::ifstream config_file(filepath);
    if (!config_file) {
        std::cerr << "Error: Could not open config file: " << filepath << std::endl;
        return false;
    }

    try {
        json data = json::parse(config_file);

        // Parse basic settings
        out_config.hostname = data.value("hostname", "mun-os-container");
        out_config.rootfs_path = data.at("rootfs_path");
        out_config.command = data.at("command");
        if (data.contains("args")) {
            out_config.args = data["args"].get<std::vector<std::string>>();
        }

        // Parse resource limits
        if (data.contains("resources")) {
            out_config.memory_limit_mb = data["resources"].value("memory_limit_mb", 0);
            out_config.process_limit = data["resources"].value("process_limit", 0);
        }
        
        // Parse new security settings from the "security" object
        if (data.contains("security")) {
            const auto& sec = data["security"];
            auto& sec_config = out_config.security;

            sec_config.use_pivot_root = sec.value("use_pivot_root", true);
            sec_config.readonly_rootfs = sec.value("readonly_rootfs", false);
            sec_config.use_user_namespace = sec.value("use_user_namespace", false);
            sec_config.drop_capabilities = sec.value("drop_capabilities", true);
            sec_config.use_seccomp = sec.value("use_seccomp", true);
            sec_config.seccomp_profile = sec.value("seccomp_profile", "default");
            
            // Example for UID mappings (a real implementation would parse the array)
            if (sec_config.use_user_namespace) {
                sec_config.uid_mappings.push_back({0, 1000, 1});
                sec_config.gid_mappings.push_back({0, 1000, 1});
            }
        }

    } catch (const json::exception& e) {
        std::cerr << "Error: Failed to parse JSON config: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool ConfigParser::validate(const Config& config) {
    if (config.rootfs_path.empty() || config.command.empty()) {
        std::cerr << "Validation Error: 'rootfs_path' and 'command' are required." << std::endl;
        return false;
    }
    return true;
}

