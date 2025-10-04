#include "StateManager.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>

using json = nlohmann::json;

StateManager::StateManager() {
    const char* home = getenv("HOME");
    if (home == nullptr) {
        std::cerr << "Error: HOME environment variable not set." << std::endl;
        // Fallback or exit
        exit(1);
    }
    state_base_path_ = std::string(home) + "/.mun-os/state";

    // Create the base directory if it doesn't exist
    mkdir((std::string(home) + "/.mun-os").c_str(), 0755);
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

    state_file << j.dump(4); // pretty print
    return true;
}

// Implement load_state, list_containers, and remove_state for the next sprint
bool StateManager::load_state(const std::string& container_name, ContainerState& out_state) {
    // To be implemented in Sprint 2b
    return false;
}

std::vector<ContainerState> StateManager::list_containers() {
    // To be implemented in Sprint 2b
    return {};
}

bool StateManager::remove_state(const std::string& container_name) {
    // To be implemented in Sprint 2b
    return false;
}
