cmake_minimum_required(VERSION 3.10)
project(ProjectMUN_OS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_compile_definitions(_GNU_SOURCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++fs")

include_directories(include)

# Make sure all necessary source files are listed here
add_executable(mun_os
    src/main.cpp
    src/Container.cpp
    src/CgroupManager.cpp
    src/ConfigParser.cpp
    src/StateManager.cpp
    src/Security.cpp
    src/NetworkManager.cpp
)

# Link against the capability library for CapabilityManager
target_link_libraries(mun_os PRIVATE cap)
