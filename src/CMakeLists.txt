# MPI
find_package(MPI REQUIRED)
include_directories(${MPI_C_INCLUDE_PATH})

# libpfprof
set(serial "0.2.0")
set(soserial "2")
add_library(pfprof SHARED pfprof.cc mpi_wrapper.cc mpi_f_wrapper.cc)
target_link_libraries(pfprof ${MPI_C_LIBRARIES} ${CPR_LIBRARIES})
set_target_properties(pfprof PROPERTIES VERSION ${serial} SOVERSION ${soserial})
