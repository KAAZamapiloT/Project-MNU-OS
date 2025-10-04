cmake_minimum_required(VERSION 3.10)

# Project name and version
project(ProjectMUN_OS VERSION 0.1 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debugging symbols, which is crucial for using GDB
set(CMAKE_BUILD_TYPE Debug)

# Add a compile definition for _GNU_SOURCE consistently across all files
add_compile_definitions(_GNU_SOURCE)

# Tell CMake where to find our header files (.h files)
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add the executable and list all of its source files
add_executable(mun_os
    src/main.cpp
    src/Container.cpp
    src/FilesystemManager.cpp
    src/CgroupManager.cpp
)

