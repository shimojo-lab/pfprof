#include <mpi.h>

#include "pfprof.hpp"

extern "C" void mpi_finalize_(MPI_Fint *ierr)
{
    int c_ierr = MPI_Finalize();
    if (NULL != ierr) *ierr = c_ierr;
}

extern "C" void mpi_init_(MPI_Fint *ierr)
{
    int argc = 0;
    char **argv = NULL;

    int c_ierr = MPI_Init(&argc, &argv);
    if (NULL != ierr) *ierr = c_ierr;
}

extern "C" void mpi_comm_create_(MPI_Fint *comm, MPI_Fint *group,
                                  MPI_Fint *newcomm, MPI_Fint *ierr)
{
    MPI_Comm c_newcomm;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);
    MPI_Group c_group = PMPI_Group_f2c(*group);

    int c_ierr = MPI_Comm_create(c_comm, c_group, &c_newcomm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *newcomm = PMPI_Comm_c2f(c_newcomm);
    }
}

extern "C" void mpi_comm_dup_(MPI_Fint *comm, MPI_Fint *newcomm, MPI_Fint *ierr)
{
    MPI_Comm c_newcomm;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);

    int c_ierr = MPI_Comm_dup(c_comm, &c_newcomm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *newcomm = PMPI_Comm_c2f(c_newcomm);
    }
}

extern "C" void mpi_comm_split_(MPI_Fint *comm, MPI_Fint *color, MPI_Fint *key,
                                MPI_Fint *newcomm, MPI_Fint *ierr)
{
    MPI_Comm c_newcomm;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);

    int c_ierr = MPI_Comm_split(c_comm, *color, *key, &c_newcomm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *newcomm = PMPI_Comm_c2f(c_newcomm);
    }
}

extern "C" void mpi_comm_free_(MPI_Fint *comm, MPI_Fint *ierr)
{
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);

    int c_ierr = MPI_Comm_free(&c_comm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *comm = PMPI_Comm_c2f(c_comm);
    }
}
