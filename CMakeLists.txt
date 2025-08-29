cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(MNU_OS VERSION 0.1)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the executable
add_executable(MNU_OS src/main.cpp)
