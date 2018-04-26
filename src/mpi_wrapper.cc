#include <mpi.h>

#include "pfprof.hpp"

extern "C" int MPI_Init(int *argc, char ***argv)
{
    int ret = PMPI_Init(argc, argv);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    return pfprof::initialize();
}

extern "C" int MPI_Init_thread(int *argc, char ***argv, int required,
                               int *provided)
{
    int ret = PMPI_Init_thread(argc, argv, required, provided);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    return pfprof::initialize();
}

extern "C" int MPI_Comm_create(MPI_Comm comm, MPI_Group group,
                               MPI_Comm *newcomm)
{
    int ret = PMPI_Comm_create(comm, group, newcomm);
    if (ret != MPI_SUCCESS || *newcomm == MPI_COMM_NULL) {
        return ret;
    }

    pfprof::register_comm(*newcomm);

    return pfprof::register_event_handlers(*newcomm,
                                          pfprof::peruse_event_handler);
}

extern "C" int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int ret = PMPI_Comm_dup(comm, newcomm);
    if (ret != MPI_SUCCESS || *newcomm == MPI_COMM_NULL) {
        return ret;
    }

    pfprof::register_comm(*newcomm);

    return pfprof::register_event_handlers(*newcomm,
                                          pfprof::peruse_event_handler);
}

extern "C" int MPI_Comm_split(MPI_Comm comm, int color, int key,
                              MPI_Comm *newcomm)
{
    int ret = PMPI_Comm_split(comm, color, key, newcomm);
    if (ret != MPI_SUCCESS || *newcomm == MPI_COMM_NULL) {
        return ret;
    }

    pfprof::register_comm(*newcomm);

    return pfprof::register_event_handlers(*newcomm,
                                          pfprof::peruse_event_handler);
}

extern "C" int MPI_Comm_free(MPI_Comm *comm)
{
    MPI_Comm cm = *comm;

    int ret = PMPI_Comm_free(comm);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    pfprof::unregister_comm(*comm);

    return pfprof::remove_event_handlers(*comm);
}

extern "C" int MPI_Finalize()
{
    pfprof::finalize();

    return PMPI_Finalize();
}
