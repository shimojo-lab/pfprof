# liboxton
set(serial "0.2.0")
set(soserial "2")
add_library(oxton SHARED oxton.cc)
target_link_libraries(oxton ${MPI_C_LIBRARIES})
set_target_properties(oxton PROPERTIES VERSION ${serial} SOVERSION ${soserial})
