#ifndef FILESYSTEM_MANAGER_H
#define FILESYSTEM_MANAGER_H

#include <string>

/**
 * @class FilesystemManager
 * @brief Manages the setup of the container's root filesystem.
 */
class FilesystemManager {
public:
    /**
     * @brief Constructs a FilesystemManager.
     * @param rootfs_path The path to the directory that will become the container's root.
     */
    FilesystemManager(const std::string& rootfs_path);

    /**
     * @brief Sets up the filesystem environment for the container.
     * This includes chroot, changing directory, and mounting /proc.
     * @return true on success, false on failure.
     */
    bool setup();

private:
    std::string rootfs_path_;

    /**
     * @brief Mounts the /proc filesystem inside the new root.
     * @return true on success, false on failure.
     */
    bool mount_proc();
};

#endif // FILESYSTEM_MANAGER_H
