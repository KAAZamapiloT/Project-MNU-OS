#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <vector>
#include <unistd.h> // For pid_t

// Define _GNU_SOURCE to get the declaration of clone()
#define _GNU_SOURCE
#include <sched.h>

/**
 * @class Process
 * @brief Encapsulates the logic for creating and running a new process using clone().
 *
 * This class manages the creation of a child process, including its stack,
 * and executes a specified command within it. It is designed to be the
 * foundation for a container, allowing for namespace flags to be passed
 * to the clone() system call.
 */
class Process {
public:
    /**
     * @brief Constructs a Process object.
     * @param command The absolute path to the executable to run.
     * @param args A vector of string arguments for the command.
     */
    Process(const std::string& command, const std::vector<std::string>& args = {});

    /**
     * @brief Runs the process in a new child created by clone().
     * @param flags A bitmask of flags to be passed to clone() (e.g., CLONE_NEWPID).
     * Defaults to SIGCHLD to ensure the parent is notified on exit.
     * @return The exit status of the child process, or -1 on failure.
     */
    int run(int flags = SIGCHLD);

private:
    /**
     * @brief The static entry point for the child process created by clone().
     * This function is responsible for setting up and executing the user's command.
     * @param arg A pointer to the Process object (`this`).
     * @return The exit code of the child process.
     */
    static int child_entry_point(void* arg);

    std::string command_;
    std::vector<std::string> args_;
};

#endif // PROCESS_H

