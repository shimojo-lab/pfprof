#include <mpi.h>
#include <stdio.h>

char buf[10000];

int main(int argc, char **argv) {
    // Initialize the MPI environment
    MPI_Init(NULL, NULL);

    // Get the number of processes
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Get the rank of the process
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MPI_Bcast(buf, 123, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* MPI_Comm newcomm; */
    /* MPI_Comm_split(MPI_COMM_WORLD, world_rank % 2, world_rank / 2, &newcomm); */

    /* MPI_Bcast(buf, 1000, MPI_CHAR, 0, newcomm); */

    /* int rank; */
    /* MPI_Comm_rank(newcomm, &rank); */
    /* MPI_Request req; */
    /* MPI_Isend(buf, 123, MPI_CHAR, (rank + 1) % 8, 0, newcomm, &req); */
    /* MPI_Recv(buf, 123, MPI_CHAR, (rank + 8 - 1) % 8, 0, newcomm, NULL); */
    /* MPI_Wait(&req, NULL); */

    // Finalize the MPI environment.
    MPI_Finalize();
}
