#include <mpi.h>
#include <stdio.h>

char buf[1000];

int main(int argc, char** argv) {
    // Initialize the MPI environment
    MPI_Init(NULL, NULL);

    // Get the number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Get the rank of the process
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MPI_Bcast(buf, 1000, MPI_CHAR, 0, MPI_COMM_WORLD);

    MPI_Comm newcomm;
    MPI_Comm_split(MPI_COMM_WORLD, world_rank % 2, world_rank / 2, &newcomm);

    // Finalize the MPI environment.
    MPI_Finalize();
}
