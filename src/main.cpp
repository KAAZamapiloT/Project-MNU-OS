#include "Container.h"
#include "Config.h"
#include "ConfigParser.h"
#include <iostream>
#include <vector>
#include <string>

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [options] <command> [args...]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --config <path>      Optional. Path to a JSON configuration file." << std::endl;
    std::cerr << "  --rootfs <path>      Required (unless provided in config). Path to the container's root filesystem." << std::endl;
    std::cerr << "  --memory <mb>        Optional. Memory limit in megabytes. Overrides config file." << std::endl;
    std::cerr << "  --pids <limit>       Optional. Maximum number of processes. Overrides config file." << std::endl;
    std::cerr << "\nExample (CLI only): " << prog_name << " --rootfs ./alpine --memory 256 /bin/sh" << std::endl;
    std::cerr << "Example (JSON only): " << prog_name << " --config configs/alpine.json" << std::endl;
    std::cerr << "Example (Hybrid): " << prog_name << " --config configs/alpine.json --memory 128" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    Config config; // Start with default values
    std::vector<std::string> cli_args;
    for (int i = 1; i < argc; ++i) {
        cli_args.push_back(argv[i]);
    }

    // --- Phase 1: Parse config file if it exists ---
    std::string config_path;
    for (size_t i = 0; i < cli_args.size(); ++i) {
        if (cli_args[i] == "--config" && i + 1 < cli_args.size()) {
            config_path = cli_args[i+1];
            break;
        }
    }

    if (!config_path.empty()) {
        std::cout << "[Main] Loading base configuration from: " << config_path << std::endl;
        if (!ConfigParser::parse(config_path, config)) {
            return 1; // Parser prints its own error
        }
    }

    // --- Phase 2: Parse CLI flags to override config values ---
    int command_start_index = -1;
    for (size_t i = 0; i < cli_args.size(); ++i) {
        std::string arg = cli_args[i];
        if (arg == "--config") {
            i++; // Skip the path
            continue;
        } else if (arg == "--rootfs") {
            if (i + 1 < cli_args.size()) config.rootfs_path = cli_args[++i];
        } else if (arg == "--memory") {
            if (i + 1 < cli_args.size()) config.memory_limit_mb = std::stoi(cli_args[++i]);
        } else if (arg == "--pids") {
            if (i + 1 < cli_args.size()) config.process_limit = std::stoi(cli_args[++i]);
        } else {
            // First non-flag argument is the command
            command_start_index = i;
            break;
        }
    }
    
    // --- Phase 3: Populate command and its args ---
    // If a command was found on the CLI, it overrides the one from the JSON file.
    if (command_start_index != -1) {
        config.command = cli_args[command_start_index];
        config.args.clear(); // Clear args from JSON if a new command is provided
        for (size_t i = command_start_index + 1; i < cli_args.size(); ++i) {
            config.args.push_back(cli_args[i]);
        }
    }

    // --- Phase 4: Validate the final configuration ---
    if (!ConfigParser::validate(config)) {
        print_usage(argv[0]);
        return 1;
    }

    // --- Create and Run the Container ---
    std::cout << "[Main] Starting container..." << std::endl;
    Container container(config);
    int exit_code = container.run();

    std::cout << "[Main] Container finished with exit code: " << exit_code << std::endl;

    return exit_code;
}

