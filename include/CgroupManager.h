#ifndef CGROUP_MANAGER_H
#define CGROUP_MANAGER_H

#include <string>
#include <unistd.h> // For pid_t
#include "Config.h" // Depends on the Config struct

/**
 * @class CgroupManager
 * @brief Manages cgroup setup, application, and teardown for a container.
 */
class CgroupManager {
public:
    /**
     * @brief Constructs a CgroupManager.
     * @param config The container's configuration containing resource limits.
     */
    CgroupManager(const Config& config);

    /**
     * @brief Creates the cgroup directory and writes the resource limits.
     * @return true on success, false on failure.
     */
    bool setup();

    /**
     * @brief Moves the specified process ID into the cgroup to enforce limits.
     * @param pid The process ID of the container.
     * @return true on success, false on failure.
     */
    bool apply(pid_t pid);

    /**
     * @brief Cleans up by removing the cgroup directory after the container exits.
     */
    void teardown();

private:
    const Config& config_;
    std::string cgroup_path_;
};

#endif // CGROUP_MANAGER_H
