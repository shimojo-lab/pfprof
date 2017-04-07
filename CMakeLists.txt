# CMake version
cmake_minimum_required(VERSION 2.8)

# Compiler options
set(cmake_c_flags_release "-Wall -Werror -O3")
set(cmake_c_flags_debug "-g")
set(cmake_build_type Release)
add_compile_options(-std=c++11)

# Project
project(oxton C CXX)

# Include paths and source code directories
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})
add_subdirectory(src)
