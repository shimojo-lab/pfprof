# CMake version
cmake_minimum_required(VERSION 3.1)

# Compiler options
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(cmake_c_flags_release "-Wall -Werror -O3")
set(cmake_c_flags_debug "-g")
set(cmake_build_type Release)

# Project
project(oxton C CXX)

# Include paths and source code directories
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})
add_subdirectory(src)
