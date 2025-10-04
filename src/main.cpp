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

    if (command == "start") {
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

    // Print a header
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
        // Optionally clean up the state file here if it's a zombie
        // state_manager.remove_state(container_name);
        return;
    }

    std::cout << "Stopping container '" << container_name << "' (PID: " << state.pid << ")..." << std::endl;

    // Send a graceful termination signal first
    if (kill(state.pid, SIGTERM) != 0) {
        std::perror("kill (SIGTERM)");
        // If SIGTERM fails, we might try SIGKILL, but for now, we'll just report.
    }

    // In a real runtime, you'd wait a few seconds and then send SIGKILL if it's still alive.
    // For now, we'll just remove the state.

    // Clean up the state directory
    if (state_manager.remove_state(container_name)) {
        std::cout << "Container '" << container_name << "' stopped and state removed." << std::endl;
    } else {
        std::cerr << "Warning: Container process may have been stopped, but failed to remove state." << std::endl;
    }
}


void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <command> [options]" << std::endl;
    std::cerr << "Commands:" << std::endl;
    std::cerr << "  start <path_to_config.json>   Start a new container in the background." << std::endl;
    std::cerr << "  list                          List all running containers." << std::endl;
    std::cerr << "  stop <container_name>         Stop a running container." << std::endl;
}

