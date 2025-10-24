#include "Config.h"
#include "ConfigParser.h"
#include "StateManager.h"
#include "Container.h"      // <-- FIX: This was the missing include for the Container class
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <sys/mount.h> // <-- NEW: Include for mount()
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
void handle_kill_all_command();
void handle_cleanup_command();
void handle_prune_command();
void handle_name_command();
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
    }else if (command == "kill-all") {
    handle_kill_all_command();
}
else if (command == "cleanup") {
    handle_cleanup_command();
}
else if (command == "prune") {
    handle_prune_command();
}
else {
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
        std::cerr << "Error: Container '" << container_name << "' is not running." << std::endl;
        return;
    }

    pid_t container_pid = state_opt->pid;

    // ✅ FIX: Check if nsenter binary exists before attempting to use it
    // BEFORE: Directly called nsenter, would fail at exec time with confusing error
    // AFTER: Validate binary exists and is executable, provide helpful error message
    const char* nsenter_path = "/usr/bin/nsenter";
    if (access(nsenter_path, X_OK) != 0) {
        std::cerr << "Error: nsenter not found at " << nsenter_path << std::endl;
        std::cerr << "Install util-linux package: sudo apt install util-linux" << std::endl;
        return;
    }

    // Build nsenter command with absolute path
    std::vector<std::string> nsenter_args{
        nsenter_path,
        "--target", std::to_string(container_pid),
        "--mount", "--uts", "--ipc", "--net", "--pid"
    };

    // Add user's command
    for (int i = 3; i < argc; i++) {
        nsenter_args.push_back(argv[i]);
    }

    // Convert to C-style args
    std::vector<char*> c_args;
    for (auto& arg : nsenter_args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    std::cout << "[Exec] Executing command in container '" << container_name << "'..." << std::endl;

    // Execute nsenter
    pid_t exec_pid = fork();
    if (exec_pid == -1) {
        std::perror("fork");
        return;
    }

    if (exec_pid == 0) {
        // Child process
        execvp(c_args[0], c_args.data());
        std::cerr << "Error: Failed to execute nsenter: " << strerror(errno) << std::endl;
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(exec_pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            std::cout << "[Exec] Command completed successfully." << std::endl;
        } else {
            std::cout << "[Exec] Command exited with status: " << WEXITSTATUS(status) << std::endl;
        }
    }
}


void handle_kill_all_command() {
    StateManager state_manager;
    auto containers = state_manager.list_containers();

    if (containers.empty()) {
        std::cout << "No containers found." << std::endl;
        return;
    }

    std::cout << "Stopping all running containers..." << std::endl;
    int stopped_count = 0;
    int already_stopped = 0;

    for (const auto& state : containers) {
        if (state.status == "running") {
            std::cout << "  Stopping '" << state.name << "'..." << std::endl;
            handle_stop_command(state.name);
            stopped_count++;
        } else {
            already_stopped++;
        }
    }

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Stopped: " << stopped_count << " container(s)" << std::endl;
    std::cout << "Already stopped: " << already_stopped << " container(s)" << std::endl;

    if (already_stopped > 0) {
        std::cout << "\nTip: Use 'prune' to remove all stopped containers." << std::endl;
    }
}

// New function: Stop all running AND remove all stopped containers
void handle_cleanup_command() {
    StateManager state_manager;
    auto containers = state_manager.list_containers();

    if (containers.empty()) {
        std::cout << "No containers found." << std::endl;
        return;
    }

    std::cout << "Cleaning up all containers..." << std::endl;
    int stopped_count = 0;
    int removed_count = 0;

    // First pass: Stop all running containers
    for (const auto& state : containers) {
        if (state.status == "running") {
            std::cout << "  Stopping '" << state.name << "'..." << std::endl;
            handle_stop_command(state.name);
            stopped_count++;
        }
    }

    // Refresh the list after stopping
    containers = state_manager.list_containers();

    // Second pass: Remove all (now stopped) containers
    for (const auto& state : containers) {
        std::cout << "  Removing '" << state.name << "'..." << std::endl;
        if (state_manager.remove_state(state.name)) {
            removed_count++;
        }
    }

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Stopped: " << stopped_count << " container(s)" << std::endl;
    std::cout << "Removed: " << removed_count << " container(s)" << std::endl;
    std::cout << "All containers cleaned up successfully!" << std::endl;
}

// New function: Remove only stopped containers (like Docker prune)
void handle_prune_command() {
    StateManager state_manager;
    auto containers = state_manager.list_containers();

    if (containers.empty()) {
        std::cout << "No containers found." << std::endl;
        return;
    }

    std::vector<std::string> stopped_containers;
    int running_count = 0;

    // Identify stopped containers
    for (const auto& state : containers) {
        if (state.status == "stopped") {
            stopped_containers.push_back(state.name);
        } else {
            running_count++;
        }
    }

    if (stopped_containers.empty()) {
        std::cout << "No stopped containers to remove." << std::endl;
        if (running_count > 0) {
            std::cout << running_count << " running container(s) left untouched." << std::endl;
        }
        return;
    }

    std::cout << "Removing " << stopped_containers.size() << " stopped container(s)..." << std::endl;
    int removed_count = 0;

    for (const auto& name : stopped_containers) {
        std::cout << "  Removing '" << name << "'..." << std::endl;
        if (state_manager.remove_state(name)) {
            removed_count++;
        }
    }

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Removed: " << removed_count << " stopped container(s)" << std::endl;
    if (running_count > 0) {
        std::cout << "Running: " << running_count << " container(s) (left untouched)" << std::endl;
    }
}

// Update the main() function to include the new commands
// Add these cases in your command routing section:



void parse_cli_and_env(int start_index, int argc, char* argv[], Config& config) {
    // Phase 1: Parse Environment Variables with validation

    // ✅ FIX: Add try-catch and bounds checking to prevent integer overflow
    // BEFORE: config.memory_limit_mb = std::stoi(mem_env); // Can crash or overflow!
    // AFTER: Validate range 0-1000000 MB (reasonable limit ~1TB)
    if (const char* mem_env = std::getenv("MUN_OS_MEMORY_LIMIT")) {
        try {
            int val = std::stoi(mem_env);
            if (val >= 0 && val <= 1000000) {
                config.memory_limit_mb = val;
            } else {
                std::cerr << "Warning: Memory limit out of range (0-1000000 MB), ignoring" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid memory limit value: " << mem_env << std::endl;
        }
    }

    if (const char* pid_env = std::getenv("MUN_OS_PROCESS_LIMIT")) {
        try {
            int val = std::stoi(pid_env);
            if (val >= 0 && val <= 100000) {
                config.process_limit = val;
            } else {
                std::cerr << "Warning: Process limit out of range (0-100000), ignoring" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid process limit value: " << pid_env << std::endl;
        }
    }

    // Phase 2: Parse CLI arguments (overrides environment variables)
    for (int i = start_index; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--rootfs" && i + 1 < argc) {
            config.rootfs_path = argv[++i];
        } else if (arg == "--hostname" && i + 1 < argc) {
            config.hostname = argv[++i];
        } else if (arg == "--memory" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val >= 0 && val <= 1000000) {
                    config.memory_limit_mb = val;
                } else {
                    std::cerr << "Warning: Memory limit out of range, ignoring" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid memory value" << std::endl;
            }
        } else if (arg == "--pids" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val >= 0 && val <= 100000) {
                    config.process_limit = val;
                } else {
                    std::cerr << "Warning: Process limit out of range, ignoring" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid process limit value" << std::endl;
            }
        } else {
            // This is the command and its arguments
            config.command = arg;
            for (int j = i + 1; j < argc; j++) {
                config.args.push_back(argv[j]);
            }
            break;
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
    std::string custom_name;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }else if (std::string(argv[i]) == "--name" && i + 1 < argc) {
                    custom_name = argv[i + 1];  // ✅ NEW: Capture custom name
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
    std::string container_name = custom_name.empty()
           ? std::filesystem::path(config_path).stem().string()
           : custom_name;
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

    auto state = *state_opt;

    if (state.status == "stopped") {
        std::cout << "Container '" << container_name << "' is already stopped." << std::endl;
        return;
    }

    std::cout << "Stopping container '" << container_name << "'..." << std::endl;

    if (kill(state.pid, SIGTERM) == 0) {
        std::cout << "Sent SIGTERM, waiting for graceful shutdown..." << std::endl;

        // ✅ FIX: Use waitpid(WNOHANG) instead of kill(pid, 0) polling
        // BEFORE: for loop with kill(pid, 0) had race window - process could
        //         exit between checks, leaving zombie process
        // AFTER: Proper waitpid() with WNOHANG flag to reap child immediately
        int status;
        bool exited = false;

        for (int i = 0; i < 10; i++) {
            pid_t result = waitpid(state.pid, &status, WNOHANG);
            if (result == state.pid) {
                std::cout << "Process exited gracefully" << std::endl;
                exited = true;
                break;
            } else if (result == -1) {
                if (errno == ECHILD) {
                    // Process already reaped or doesn't exist
                    exited = true;
                    break;
                }
                perror("waitpid");
                break;
            }
            usleep(500000);  // 500ms between checks
        }

        // Force kill if still alive after timeout
        if (!exited && kill(state.pid, 0) == 0) {
            std::cout << "Container did not stop, sending SIGKILL..." << std::endl;
            kill(state.pid, SIGKILL);
            waitpid(state.pid, nullptr, 0);  // Clean up zombie
        }
    } else {
        if (errno == ESRCH) {
            std::cout << "Process already terminated." << std::endl;
        } else {
            std::cerr << "Failed to send SIGTERM: " << strerror(errno) << std::endl;
            return;
        }
    }

    // Update state
    state.status = "stopped";
    if (!state_manager.save_state(state)) {
        std::cerr << "Warning: Failed to update container state." << std::endl;
    } else {
        std::cout << "Container '" << container_name << "' stopped successfully." << std::endl;
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
    std::cerr << "  remove <container_name>       Remove a stopped container." << std::endl;
    std::cerr << "  kill-all                      Stop all running containers." << std::endl;
    std::cerr << "  prune                         Remove all stopped containers." << std::endl;
    std::cerr << "  cleanup                       Stop all running AND remove all containers." << std::endl;
    std::cerr << "\nOptions:" << std::endl;  // ✅ NEW section
    std::cerr << "  --name <name>         Custom container name (overrides default)" << std::endl;
    std::cerr << "  --rootfs <path>       Path to root filesystem" << std::endl;
    std::cerr << "  --hostname <name>     Container hostname" << std::endl;
    std::cerr << "  --memory <MB>         Memory limit in megabytes" << std::endl;
    std::cerr << "  --pids <count>        Maximum number of processes" << std::endl;
    std::cerr << "  --config <path>       JSON configuration file" << std::endl;
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
