cmake_minimum_required(VERSION 3.10)
project(ProjectMUN_OS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_compile_definitions(_GNU_SOURCE)
# We need C++17 for std::filesystem
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs")

include_directories(include)

add_executable(mun_os
    src/main.cpp
    src/Container.cpp
    src/FilesystemManager.cpp
    src/CgroupManager.cpp
    src/ConfigParser.cpp
    src/StateManager.cpp
)

