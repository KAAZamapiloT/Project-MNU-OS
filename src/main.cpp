#include "Container.h"
#include "Config.h"
#include "ConfigParser.h"
#include "StateManager.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

// Forward declarations for command handlers
void handle_start_command(const std::string& config_path);
void print_usage(const char* prog_name);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "start") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " start <path_to_config.json>" << std::endl;
            return 1;
        }
        handle_start_command(argv[2]);
    } else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

void handle_start_command(const std::string& config_path) {
    Config config;
    if (!ConfigParser::parse(config_path, config) || !ConfigParser::validate(config)) {
        return;
    }

    Container container(config);
    pid_t child_pid = container.start();

    if (child_pid > 0) {
        StateManager state_manager;
        ContainerState new_state;
        new_state.name = std::filesystem::path(config_path).stem().string();
        new_state.pid = child_pid;
        new_state.status = "running";
        new_state.config_path = config_path;

        if (!state_manager.save_state(new_state)) {
            std::cerr << "Error: Failed to save container state." << std::endl;
            // In a real scenario, you would kill the child process here
        }
    }
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <command> [options]" << std::endl;
    std::cerr << "Commands:" << std::endl;
    std::cerr << "  start <path_to_config.json>   Create and start a new container in the background." << std::endl;
}

