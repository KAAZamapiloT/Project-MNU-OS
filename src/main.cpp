#include "Container.h"
#include "Config.h"
#include "ConfigParser.h"
#include "StateManager.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <signal.h> // For kill()

// Forward declarations for command handlers
void handle_run_command(int argc, char* argv[]);
void handle_start_command(const std::string& config_path);
void handle_list_command();
void handle_stop_command(const std::string& container_name);
void print_usage(const char* prog_name);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "run") {
        handle_run_command(argc, argv);
    } else if (command == "start") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " start <path_to_config.json>" << std::endl;
            return 1;
        }
        handle_start_command(argv[2]);
    } else if (command == "list") {
        handle_list_command();
    } else if (command == "stop") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " stop <container_name>" << std::endl;
            return 1;
        }
        handle_stop_command(argv[2]);
    } else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

void handle_run_command(int argc, char* argv[]) {
    Config config;
    int command_start_index = -1;

    // Parse all arguments after "run"
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rootfs") {
            if (i + 1 < argc) config.rootfs_path = argv[++i];
        } else if (arg == "--memory") {
            if (i + 1 < argc) config.memory_limit_mb = std::stoi(argv[++i]);
        } else if (arg == "--pids") {
            if (i + 1 < argc) config.process_limit = std::stoi(argv[++i]);
        } else if (arg == "--config") {
            // A config file can be used with run, but CLI flags will override it
             if (i + 1 < argc) {
                if (!ConfigParser::parse(argv[++i], config)) return;
             }
        } else {
            // The first non-flag argument is the start of the command
            command_start_index = i;
            break;
        }
    }

    // Populate command and its args from the CLI, overriding the JSON file if present
    if (command_start_index != -1) {
        config.command = argv[command_start_index];
        config.args.clear();
        for (int i = command_start_index + 1; i < argc; ++i) {
            config.args.push_back(argv[i]);
        }
    }

    // After parsing everything, validate the final configuration.
    // This now correctly checks the config state whether it came from a file or the CLI.
    if (!ConfigParser::validate(config)) {
        print_usage(argv[0]);
        return;
    }

    std::cout << "[Main] Starting container in foreground mode..." << std::endl;
    Container container(config);
    int exit_code = container.run();
    std::cout << "[Main] Container finished with exit code: " << exit_code << std::endl;
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
            kill(child_pid, SIGKILL); // Kill the orphaned process
        }
    }
}

void handle_list_command() {
    StateManager state_manager;
    auto containers = state_manager.list_containers();

    printf("%-20s %-10s %-10s %-s\n", "CONTAINER NAME", "PID", "STATUS", "CONFIG");
    printf("%-20s %-10s %-10s %-s\n", "--------------------", "----------", "----------", "--------------------");

    if (containers.empty()) {
        printf("No containers are running.\n");
    } else {
        for (const auto& state : containers) {
            printf("%-20s %-10d %-10s %-s\n",
                   state.name.c_str(),
                   state.pid,
                   state.status.c_str(),
                   state.config_path.c_str());
        }
    }
}

void handle_stop_command(const std::string& container_name) {
    StateManager state_manager;
    auto state_opt = state_manager.load_state(container_name);

    if (!state_opt) {
        std::cerr << "Error: Container '" << container_name << "' not found." << std::endl;
        return;
    }

    ContainerState state = *state_opt;

    if (state.status != "running") {
        std::cout << "Container '" << container_name << "' is already stopped." << std::endl;
        return;
    }

    std::cout << "Stopping container '" << container_name << "' (PID: " << state.pid << ")..." << std::endl;
    
    if (kill(state.pid, SIGTERM) != 0) {
        std::perror("kill (SIGTERM)");
    }
    
    if (state_manager.remove_state(container_name)) {
        std::cout << "Container '" << container_name << "' stopped and state removed." << std::endl;
    } else {
        std::cerr << "Warning: Failed to remove state for container '" << container_name << "'." << std::endl;
    }
}


void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <command> [options]" << std::endl;
    std::cerr << "Commands:" << std::endl;
    std::cerr << "  run [options] <command>...    Create and run a new container in the foreground." << std::endl;
    std::cerr << "  start <path_to_config.json>   Start a new container in the background." << std::endl;
    std::cerr << "  list                          List all running containers." << std::endl;
    std::cerr << "  stop <container_name>         Stop a running container." << std::endl;
}

// Example usage for 'run' command:
// ./container run --rootfs /path/to/rootfs --memory 512 --pids
// ./container run --config /path/to/config.json --memory 256 /bin/bash -c "echo Hello, World!"
// Example usage for '


//  sudo ./build/mun_os run --config configs/example.json

// sudo ./build/mun_os run --rootfs ./rootfs --memory 128 --pids 10 /bin/sh

// sudo ./build/mun_os start configs/bg.json

// sudo ./build/mun_os list

// sudo ./build/mun_os stop bg