#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <string>
#include <vector>
#include <unistd.h> // For pid_t

struct ContainerState {
    std::string name;
    pid_t pid;
    std::string status;
    std::string config_path;
};

class StateManager {
public:
    StateManager();
    bool save_state(const ContainerState& state);
    bool load_state(const std::string& container_name, ContainerState& out_state);
    std::vector<ContainerState> list_containers();
    bool remove_state(const std::string& container_name);

private:
    std::string state_base_path_;
};

#endif // STATE_MANAGER_H
