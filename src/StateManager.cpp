#include "StateManager.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <dirent.h>   // For reading directories
#include <sys/stat.h> // For mkdir
#include <pwd.h>      // For getpwnam (to get home dir with sudo)
#include <signal.h>   // For kill() to check if process is running
#include <cstring>    // For strerror
#include <cerrno>     // For errno

using json = nlohmann::json;

// Helper function to get the correct home directory, even with sudo
static std::string get_real_home_dir() {
    const char* sudo_user = getenv("SUDO_USER");
    if (sudo_user != nullptr) {
        struct passwd *pw = getpwnam(sudo_user);
        if (pw != nullptr) return std::string(pw->pw_dir);
    }
    const char* home = getenv("HOME");
    if (home != nullptr) return std::string(home);
    return "";
}

// Helper to check if a process with a given PID is currently running
static bool is_process_running(pid_t pid) {
    // kill(pid, 0) is the standard way to check for a process's existence.
    // It doesn't send a signal, but performs error checking.
    return (kill(pid, 0) == 0);
}

StateManager::StateManager() {
    std::string home_dir = get_real_home_dir();
    if (home_dir.empty()) {
        std::cerr << "Error: Could not determine home directory." << std::endl;
        exit(1);
    }
    state_base_path_ = home_dir + "/.mun-os/state";
    mkdir((home_dir + "/.mun-os").c_str(), 0755);
    mkdir(state_base_path_.c_str(), 0755);
}

bool StateManager::save_state(const ContainerState& state) {
    std::string container_path = state_base_path_ + "/" + state.name;
    mkdir(container_path.c_str(), 0755);

    std::ofstream state_file(container_path + "/state.json");
    if (!state_file.is_open()) return false;

    json j;
    j["name"] = state.name;
    j["pid"] = state.pid;
    j["status"] = state.status;
    j["config_path"] = state.config_path;

    state_file << j.dump(4);
    return true;
}

std::optional<ContainerState> StateManager::load_state(const std::string& container_name) {
    std::ifstream state_file(state_base_path_ + "/" + container_name + "/state.json");
    if (!state_file.is_open()) {
        return std::nullopt; // Container state not found
    }

    try {
        json j = json::parse(state_file);
        ContainerState state;
        state.name = j["name"];
        state.pid = j["pid"];
        state.status = j["status"];
        state.config_path = j["config_path"];

        // Update status based on whether the process is actually running
        if (state.status == "running" && !is_process_running(state.pid)) {
            state.status = "stopped";
            // In a more advanced version, you might re-save the state here.
        }

        return state;
    } catch (json::exception& e) {
        std::cerr << "Error parsing state for " << container_name << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<ContainerState> StateManager::list_containers() {
    std::vector<ContainerState> containers;
    DIR* dir = opendir(state_base_path_.c_str());
    if (dir == nullptr) {
        std::perror("opendir");
        return containers;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            if (auto state = load_state(entry->d_name)) {
                containers.push_back(*state);
            }
        }
    }
    closedir(dir);
    return containers;
}

bool StateManager::remove_state(const std::string& container_name) {
    std::string container_path = state_base_path_ + "/" + container_name;
    std::string state_file_path = container_path + "/state.json";

    if (remove(state_file_path.c_str()) != 0) {
        // This might fail if the file is already gone, which is okay.
        std::perror("remove state file");
    }

    if (rmdir(container_path.c_str()) != 0) {
        std::perror("rmdir container state");
        return false;
    }
    return true;
}

