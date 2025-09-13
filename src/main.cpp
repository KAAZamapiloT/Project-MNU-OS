#include "Container.h"
#include <iostream>
#include <string>
#include <vector>

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " --rootfs <path> <command> [args...]" << std::endl;
    std::cerr << "Example: " << prog_name << " --rootfs ./alpine /bin/sh" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    // --- Simple Argument Parsing ---
    std::string rootfs_path;
    std::string command;
    std::vector<std::string> args;

    // Look for --rootfs argument
    if (std::string(argv[1]) == "--rootfs") {
        rootfs_path = argv[2];
        command = argv[3];
        for (int i = 4; i < argc; ++i) {
            args.push_back(argv[i]);
        }
    } else {
        print_usage(argv[0]);
        return 1;
    }

    // --- Create and Run the Container ---
    std::cout << "[Main] Starting container..." << std::endl;
    Container container("mun-os-container", rootfs_path, command, args);
    int exit_code = container.run();

    std::cout << "[Main] Container finished with exit code: " << exit_code << std::endl;

    return exit_code;
}

