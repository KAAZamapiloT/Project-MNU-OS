#include "Config.h"
#include "ConfigParser.h"
#include "StateManager.h"
#include "Container.h"      // <-- FIX: This was the missing include for the Container class
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>         // <-- Added for sleep() and usleep()
#include <fcntl.h>          // For open()
#include <sched.h>          // For setns()
#include <cstring>          // <-- FIX: Added for strerror()
#include <cerrno>           // <-- FIX: Added for errno

// Forward declarations for command handlers
void handle_run_command(int argc, char* argv[]);
void handle_start_command(int argc, char* argv[]);
void handle_list_command();
void handle_stop_command(const std::string& container_name);
void handle_restart_command(const std::string& container_name);
void handle_remove_command(const std::string& container_name);
void handle_exec_command(const std::string& container_name, int argc, char* argv[]);
void print_usage(const char* prog_name);
void parse_cli_and_env(int start_index, int argc, char* argv[], Config& config);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "run") {
        handle_run_command(argc, argv);
    } else if (command == "start") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " start [options] --config <path>" << std::endl;
            return 1;
        }
        handle_start_command(argc, argv);
    } else if (command == "list") {
        handle_list_command();
    } else if (command == "stop") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " stop <container_name>" << std::endl;
            return 1;
        }
        handle_stop_command(argv[2]);
    } else if (command == "restart") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " restart <container_name>" << std::endl;
            return 1;
        }
        handle_restart_command(argv[2]);
    } else if (command == "remove" || command == "rm") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " remove <container_name>" << std::endl;
            return 1;
        }
        handle_remove_command(argv[2]);
    }else if (command == "exec") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " exec <container_name> <command> [args...]" << std::endl;
            return 1;
        }
        handle_exec_command(argv[2], argc, argv);
    } else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

void handle_exec_command(const std::string& container_name, int argc, char* argv[]) {
    StateManager state_manager;
    auto state_opt = state_manager.load_state(container_name);

    if (!state_opt || state_opt->status != "running") {
        std::cerr << "Error: Container '" << container_name << "' is not running or does not exist." << std::endl;
        return;
    }

    pid_t container_pid = state_opt->pid;
    std::cout << "[Exec] Entering namespaces of container '" << container_name << "' (PID: " << container_pid << ")..." << std::endl;

    const std::vector<std::string> ns_types = {"pid", "mnt", "uts", "net", "ipc"};
    std::vector<int> ns_fds;

    // Get file descriptors for each of the container's namespaces
    for (const auto& ns_type : ns_types) {
        std::string ns_path = "/proc/" + std::to_string(container_pid) + "/ns/" + ns_type;
        int fd = open(ns_path.c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "Warning: Could not open namespace file " << ns_path << ": " << strerror(errno) << std::endl;
            continue;
        }
        ns_fds.push_back(fd);
    }

    // Join the namespaces
    for (size_t i = 0; i < ns_fds.size(); ++i) {
        if (setns(ns_fds[i], 0) == -1) {
            std::cerr << "Error: setns() failed for a namespace: " << strerror(errno) << std::endl;
            for (int fd : ns_fds) close(fd);
            return;
        }
    }
    
    // Close the file descriptors now that we've joined
    for (int fd : ns_fds) {
        close(fd);
    }

    std::cout << "[Exec] Successfully entered namespaces. Forking new process..." << std::endl;

    // Fork and exec the new command inside the joined namespaces
    pid_t child_pid = fork();
    if (child_pid == -1) {
        std::perror("fork");
        return;
    }

    if (child_pid == 0) {
        // --- Child Process ---
        char** command_args = &argv[3];
        execvp(command_args[0], command_args);
        
        std::perror("execvp");
        exit(1);
    } else {
        // --- Parent Process ---
        waitpid(child_pid, nullptr, 0);
        std::cout << "[Exec] Command finished." << std::endl;
    }
}


void parse_cli_and_env(int start_index, int argc, char* argv[], Config& config) {
    // Phase 1: Parse Environment Variables
    if (const char* mem_env = std::getenv("MUN_OS_MEMORY_LIMIT")) {
        config.memory_limit_mb = std::stoi(mem_env);
    }
    if (const char* pids_env = std::getenv("MUN_OS_PIDS_LIMIT")) {
        config.process_limit = std::stoi(pids_env);
    }

    // Phase 2: Parse CLI flags
    int command_start_index = -1;
    for (int i = start_index; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config") {
            i++; 
        } else if (arg == "--rootfs") {
            if (i + 1 < argc) config.rootfs_path = argv[++i];
        } else if (arg == "--memory") {
            if (i + 1 < argc) config.memory_limit_mb = std::stoi(argv[++i]);
        } else if (arg == "--pids") {
            if (i + 1 < argc) config.process_limit = std::stoi(argv[++i]);
        } else {
            if (command_start_index == -1) {
                command_start_index = i;
            }
        }
    }

    // Phase 3: Populate command from CLI if present
    if (command_start_index != -1) {
        config.command = argv[command_start_index];
        config.args.clear();
        for (int i = command_start_index + 1; i < argc; ++i) {
            config.args.push_back(argv[i]);
        }
    }
}

void handle_run_command(int argc, char* argv[]) {
    Config config;
    std::string config_path;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }
    }

    if (!config_path.empty()) {
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Error: Config file does not exist: " << config_path << std::endl;
            return;
        }
        
        std::string absolute_config_path = std::filesystem::absolute(config_path).string();
        if (!ConfigParser::parse_json(absolute_config_path, config)) return;
    }

    parse_cli_and_env(2, argc, argv, config);

    if (!ConfigParser::validate(config)) {
        print_usage(argv[0]);
        return;
    }
    
    std::cout << "[Main] Starting container in foreground mode..." << std::endl;
    Container container(config);
    container.run();
    std::cout << "[Main] Container finished." << std::endl;
}

void handle_start_command(int argc, char* argv[]) {
    Config config;
    std::string config_path;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }
    }

    if (config_path.empty()) {
        std::cerr << "Error: 'start' command requires a --config flag." << std::endl;
        print_usage(argv[0]);
        return;
    }

    if (!std::filesystem::exists(config_path)) {
        std::cerr << "Error: Config file does not exist: " << config_path << std::endl;
        return;
    }

    std::string absolute_config_path = std::filesystem::absolute(config_path).string();
    if (!ConfigParser::parse_json(absolute_config_path, config)) return;
    
    parse_cli_and_env(2, argc, argv, config);

    if (!ConfigParser::validate(config)) return;

    std::string container_name = std::filesystem::path(config_path).stem().string();

    StateManager state_manager;
    auto existing_state = state_manager.load_state(container_name);
    if (existing_state && existing_state->status == "running") {
        std::cerr << "Error: Container '" << container_name << "' is already running." << std::endl;
        return;
    }

    Container container(config);
    pid_t child_pid = container.start();

    if (child_pid > 0) {
        ContainerState new_state;
        new_state.name = container_name;
        new_state.pid = child_pid;
        new_state.status = "running";
        new_state.config_path = absolute_config_path;

        if (!state_manager.save_state(new_state)) {
            std::cerr << "Error: Failed to save container state" << std::endl;
            kill(child_pid, SIGKILL);
        } else {
            std::cout << "[Main] Container '" << container_name << "' started successfully" << std::endl;
        }
    }
}

void handle_list_command() {
    StateManager state_manager;
    auto containers = state_manager.list_containers();

    printf("%-20s %-10s %-10s %-s\n", "CONTAINER NAME", "PID", "STATUS", "CONFIG");
    printf("%-20s %-10s %-10s %-s\n", "--------------------", "----------", "----------", "--------------------");

    if (containers.empty()) {
        printf("No containers are managed. Use 'start' to create one.\n");
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

    if (state.status == "stopped") {
        std::cout << "Container '" << container_name << "' is already stopped." << std::endl;
        return;
    }

    std::cout << "Stopping container '" << container_name << "' (PID: " << state.pid << ")..." << std::endl;
    
    if (kill(state.pid, SIGTERM) == 0) {
        std::cout << "Sent SIGTERM, waiting for graceful shutdown..." << std::endl;
        for (int i = 0; i < 10; i++) {
            if (kill(state.pid, 0) != 0) break; // Process is gone
            usleep(500000); // 0.5 seconds
        }
        
        if (kill(state.pid, 0) == 0) {
            std::cout << "Container did not stop, sending SIGKILL..." << std::endl;
            kill(state.pid, SIGKILL);
            waitpid(state.pid, nullptr, 0);
        }
    } else {
        std::perror("kill (SIGTERM)");
    }
    
    state.status = "stopped";
    if (state_manager.save_state(state)) {
        std::cout << "Container '" << container_name << "' stopped." << std::endl;
    } else {
        std::cerr << "Warning: Failed to update container state." << std::endl;
    }
}

void handle_restart_command(const std::string& container_name) {
    StateManager state_manager;
    auto state_opt = state_manager.load_state(container_name);

    if (!state_opt) {
        std::cerr << "Error: Container '" << container_name << "' not found." << std::endl;
        return;
    }

    ContainerState state = *state_opt;
    std::string config_path = state.config_path;

    std::cout << "Restarting container '" << container_name << "'..." << std::endl;

    if (state.status == "running" && kill(state.pid, 0) == 0) {
        handle_stop_command(container_name);
        sleep(1); // Give a moment for the stop to complete
    }

    Config config;
    if (!ConfigParser::parse_json(config_path, config) || !ConfigParser::validate(config)) {
        std::cerr << "Error: Failed to load or validate config from: " << config_path << std::endl;
        return;
    }

    Container container(config);
    pid_t child_pid = container.start();

    if (child_pid > 0) {
        state.pid = child_pid;
        state.status = "running";
        if (state_manager.save_state(state)) {
            std::cout << "[Main] Container '" << container_name << "' restarted successfully with new PID " << child_pid << std::endl;
        } else {
            kill(child_pid, SIGKILL);
        }
    } else {
        std::cerr << "Error: Failed to restart container." << std::endl;
    }
}

void handle_remove_command(const std::string& container_name) {
    StateManager state_manager;
    auto state_opt = state_manager.load_state(container_name);

    if (!state_opt) {
        std::cerr << "Error: Container '" << container_name << "' not found." << std::endl;
        return;
    }

    ContainerState state = *state_opt;

    if (state.status == "running" && kill(state.pid, 0) == 0) {
        std::cout << "Container is still running. Please stop it before removing." << std::endl;
        return;
    }

    if (state_manager.remove_state(container_name)) {
        std::cout << "Container '" << container_name << "' removed." << std::endl;
    } else {
        std::cerr << "Error: Failed to remove container state." << std::endl;
    }
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <command> [options]" << std::endl;
    std::cerr << "Commands:" << std::endl;
    std::cerr << "  run [options] [<command>...]  Create and run a new container in the foreground." << std::endl;
    std::cerr << "  start --config <path>         Start a new container in the background." << std::endl;
    std::cerr << "  stop <container_name>         Stop a running container (preserves state)." << std::endl;
    std::cerr << "  restart <container_name>      Restart a stopped or running container." << std::endl;
    std::cerr << "  list                          List all containers." << std::endl;
    std::cerr << "  rm <container_name>           Remove a stopped container." << std::endl;
}


// Example usage for 'run' command:
// ./container run --rootfs /path/to/rootfs --memory 512 --pids
// ./container run --config /path/to/config.json --memory 256 /bin/bash -c "echo Hello, World!"
// Example usage for '


//  # Make sure you are in the Project-MNU-OS directory

// sudo ./build/mun_os run --config configs/example.json

// sudo ./build/mun_os run --rootfs ./rootfs --memory 128 --pids 10 /bin/sh

// sudo ./build/mun_os start configs/bg.json

// sudo ./build/mun_os list

// sudo ./build/mun_os stop bg



/* SCRIPT LEVEL ./scripts/generate_test_configs.sh

# Start multiple containers
sudo ./scripts/manage_containers.sh start bg
sudo ./scripts/manage_containers.sh start long_running
sudo ./scripts/manage_containers.sh quick "sleep 300"

# Monitor them live (auto-refreshes every 2 seconds)
sudo ./scripts/monitor.sh

# In another terminal, check status
sudo ./scripts/manage_containers.sh list

# Get detailed info
sudo ./scripts/monitor.sh info bg

# Restart one
sudo ./scripts/manage_containers.sh restart bg

# Stop all
sudo ./scripts/manage_containers.sh stopall 

# 1. Start a background container
sudo ./scripts/manage_containers.sh start bg

# 2. List containers
sudo ./scripts/manage_containers.sh list

# 3. Monitor in real-time (opens live view)
sudo ./scripts/monitor.sh

# Or one-time status
sudo ./scripts/monitor.sh list

# 4. Get detailed info about a container
sudo ./scripts/monitor.sh info bg

# 5. Restart container (now works even if stopped!)
sudo ./scripts/manage_containers.sh restart bg

# 6. Stop container
sudo ./scripts/manage_containers.sh stop bg

*/