#include "ConfigParser.h"
#include <fstream>
#include <iostream>
#include <filesystem>
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

        // Basic config
        out_config.hostname = data.value("hostname", "mun-os-container");
        out_config.rootfs_path = data.at("rootfs_path");
        out_config.command = data.at("command");
        if (data.contains("args")) {
            out_config.args = data["args"].get<std::vector<std::string>>();
        }

        // Resource limits
        if (data.contains("resources")) {
            out_config.memory_limit_mb = data["resources"].value("memory_limit_mb", 0);
            out_config.process_limit = data["resources"].value("process_limit", 0);
        }

        // Security settings
        if (data.contains("security")) {
            const auto& sec = data["security"];
            auto& sec_config = out_config.security;
            
            // This flag now controls which setup path is taken
            sec_config.use_pivot_root = sec.value("use_pivot_root", true);
            
            sec_config.readonly_rootfs = sec.value("readonly_rootfs", false);
            sec_config.use_user_namespace = sec.value("use_user_namespace", false);
            sec_config.drop_capabilities = sec.value("drop_capabilities", true);
            sec_config.use_seccomp = sec.value("use_seccomp", true);

            if (sec.contains("bind_mounts")) {
                for (const auto& mount : sec["bind_mounts"]) {
                    sec_config.bind_mounts.push_back({mount.at("source"), mount.at("target")});
                }
            }
            sec_config.setup_tmpfs = sec.value("setup_tmpfs", true);
            sec_config.tmpfs_size_mb = sec.value("tmpfs_size_mb", 64);
        } else {
            // Default to simple chroot if no security block is present
            out_config.security.use_pivot_root = false;
        }
        
    } catch (json::exception& e) {
        std::cerr << "Error: Failed to parse JSON config file: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool ConfigParser::validate(const Config& config) {
    if (config.rootfs_path.empty() || !std::filesystem::exists(config.rootfs_path)) {
        std::cerr << "Validation Error: 'rootfs_path' is invalid or does not exist." << std::endl;
        return false;
    }
    if (config.command.empty()) {
        std::cerr << "Validation Error: 'command' is a required field." << std::endl;
        return false;
    }
    return true;
}

