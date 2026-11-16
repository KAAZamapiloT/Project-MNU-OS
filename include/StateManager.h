#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <string>
#include <vector>
#include <unistd.h> // For pid_t
#include <optional> // For C++17 optional return

// Represents the state of a single container, read from state.json
struct ContainerState {
    std::string name;
    pid_t pid;
    std::string status;
    std::string config_path;
};

class StateManager {
public:
    StateManager();

    // Saves the state of a newly created container
    bool save_state(const ContainerState& state);

    // Loads the state of a single container by its name
    std::optional<ContainerState> load_state(const std::string& container_name);

    // Lists the state of all managed containers
    std::vector<ContainerState> list_containers();

    // Removes the state directory for a container
    bool remove_state(const std::string& container_name);

private:
    std::string state_base_path_;
};

#endif // STATE_MANAGER_H

