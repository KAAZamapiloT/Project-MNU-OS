cmake_minimum_required(VERSION 3.10)

# Project name and version
project(MNU_OS VERSION 0.1)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories (for process.h and other headers)
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add executable with sources
add_executable(MNU_OS
    src/main.cpp)
   
