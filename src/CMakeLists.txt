# liboxton
add_library(oxton oxton.c)

# oxton-test
add_executable (oxton-test oxton_test.c)
target_link_libraries(oxton-test oxton ${MPI_C_LIBRARIES})
