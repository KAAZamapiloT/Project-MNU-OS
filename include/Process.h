#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>


class Process {
public:
    Process(const std::string& command, const std::vector<std::string>& args = {})
        : command_(command), args_(args) {}

    // Run the process
    int run() {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            // Child process
            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>(command_.c_str()));

            for (auto& arg : args_) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);

            execvp(command_.c_str(), c_args.data());
            perror("execvp"); // only runs if execvp fails
            _exit(1);
        } else {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }
    }

private:
    std::string command_;
    std::vector<std::string> args_;
};