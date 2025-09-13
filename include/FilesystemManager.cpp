#include "FilesystemManager.h"
#include <iostream>
#include <sys/mount.h> // For mount()
#include <unistd.h>    // For chroot(), chdir()
#include <sys/stat.h>  // For mkdir()
#include <cerrno>      // For errno
#include <cstring>     // For strerror()

FilesystemManager::FilesystemManager(const std::string& rootfs_path)
    : rootfs_path_(rootfs_path) {}

bool FilesystemManager::setup() {
    // Change directory to the new rootfs. This is necessary for chroot to work
    // without requiring an absolute path.
    if (chdir(rootfs_path_.c_str()) != 0) {
        std::cerr << "Error: chdir to rootfs failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Set the new root directory for the process.
    if (chroot(".") != 0) {
        std::cerr << "Error: chroot failed: " << strerror(errno) << std::endl;
        return false;
    }

    // After chroot, the current directory is the old root of the chroot jail.
    // It's standard practice to change to the new "/" to avoid confusion.
    if (chdir("/") != 0) {
        std::cerr << "Error: chdir to new root failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Mount the /proc filesystem, which is essential for many tools like 'ps'.
    if (!mount_proc()) {
        return false;
    }

    return true;
}

bool FilesystemManager::mount_proc() {
    // Create the /proc directory inside our new rootfs if it doesn't exist.
    if (mkdir("/proc", 0555) != 0 && errno != EEXIST) {
        std::cerr << "Error: mkdir /proc failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Mount the virtual proc filesystem.
    // mount(source, target, fstype, flags, data)
    if (mount("proc", "/proc", "proc", 0, nullptr) != 0) {
        std::cerr << "Error: mount /proc failed: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}
