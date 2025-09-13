// main.cpp
#include "Process.h"
#include <iostream>
#include <vector>

// Add namespace flags here for testing
#define _GNU_SOURCE
#include <sched.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // Create the Process object
    Process proc(command, args);

    // --- THIS IS THE KEY CHANGE ---
    // You can now pass namespace flags directly to run()
    // Let's add our first layer of isolation!
    std::cout << "Running process with new PID and UTS namespaces..." << std::endl;
    int clone_flags = CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
    
    int exit_code = proc.run(clone_flags);

    std::cout << "Process finished with exit code: " << exit_code << std::endl;

    return exit_code;
}

