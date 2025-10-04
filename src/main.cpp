#include "Container.h"
#include "Config.h"
#include <iostream>
#include <string>
#include <vector>

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [options] <command> [args...]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --rootfs <path>      Required. Path to the container's root filesystem." << std::endl;
    std::cerr << "  --memory <mb>        Optional. Memory limit in megabytes (e.g., 512)." << std::endl;
    std::cerr << "  --pids <limit>       Optional. Maximum number of processes (e.g., 20)." << std::endl;
    std::cerr << "\nExample: " << prog_name << " --rootfs ./alpine --memory 256 --pids 10 /bin/sh" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    Config config;
    int command_start_index = -1;

    // --- Argument Parsing Loop ---
    // This loop now handles all flags and finds the start of the command.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rootfs") {
            if (i + 1 < argc) config.rootfs_path = argv[++i];
        } else if (arg == "--memory") {
            if (i + 1 < argc) config.memory_limit_mb = std::stoi(argv[++i]);
        } else if (arg == "--pids") {
            if (i + 1 < argc) config.process_limit = std::stoi(argv[++i]);
        } else {
            // The first non-flag argument is the command.
            command_start_index = i;
            break;
        }
    }

    // --- Validate Required Arguments ---
    if (config.rootfs_path.empty() || command_start_index == -1) {
        print_usage(argv[0]);
        return 1;
    }

    // --- Populate Command and its Arguments ---
    config.command = argv[command_start_index];
    for (int i = command_start_index + 1; i < argc; ++i) {
        config.args.push_back(argv[i]);
    }

    // --- Create and Run the Container ---
    std::cout << "[Main] Starting container..." << std::endl;
    // Note: Your Container class constructor must be updated to accept a Config object.
    Container container(config);
    int exit_code = container.run();

    std::cout << "[Main] Container finished with exit code: " << exit_code << std::endl;

    return exit_code;
}