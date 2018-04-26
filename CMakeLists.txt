# CMake version
cmake_minimum_required(VERSION 3.1)

# Compiler options
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS_RELEASE "-Wall -Werror -O3")
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_BUILD_TYPE Release)

# Project
project(pfprof C CXX)

add_subdirectory(src)
