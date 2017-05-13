# MPI
find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})

# cpr
set(USE_SYSTEM_CURL ON CACHE INTERNAL "" FORCE)
set(BUILD_CPR_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(cpr)
include_directories(${CPR_INCLUDE_DIRS})

# liboxton
set(serial "0.2.0")
set(soserial "2")
add_library(oxton SHARED oxton.cc mpi_wrapper.cc mpi_f_wrapper.cc)
target_link_libraries(oxton ${MPI_C_LIBRARIES} ${CPR_LIBRARIES})
set_target_properties(oxton PROPERTIES VERSION ${serial} SOVERSION ${soserial})
