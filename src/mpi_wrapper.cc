#include <mpi.h>

#include "oxton.hpp"

extern "C" int MPI_Init(int *argc, char ***argv)
{
    int ret = PMPI_Init(argc, argv);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    return oxton::initialize();
}

extern "C" int MPI_Init_thread(int *argc, char ***argv, int required,
                               int *provided)
{
    int ret = PMPI_Init_thread(argc, argv, required, provided);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    return oxton::initialize();
}

extern "C" int MPI_Comm_create(MPI_Comm comm, MPI_Group group,
                               MPI_Comm *newcomm)
{
    int ret = PMPI_Comm_create(comm, group, newcomm);
    if (ret != MPI_SUCCESS || *newcomm == MPI_COMM_NULL) {
        return ret;
    }

    oxton::register_comm(*newcomm);

    return oxton::register_event_handlers(*newcomm,
                                          oxton::peruse_event_handler);
}

extern "C" int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int ret = PMPI_Comm_dup(comm, newcomm);
    if (ret != MPI_SUCCESS || *newcomm == MPI_COMM_NULL) {
        return ret;
    }

    oxton::register_comm(*newcomm);

    return oxton::register_event_handlers(*newcomm,
                                          oxton::peruse_event_handler);
}

extern "C" int MPI_Comm_split(MPI_Comm comm, int color, int key,
                              MPI_Comm *newcomm)
{
    int ret = PMPI_Comm_split(comm, color, key, newcomm);
    if (ret != MPI_SUCCESS || *newcomm == MPI_COMM_NULL) {
        return ret;
    }

    oxton::register_comm(*newcomm);

    return oxton::register_event_handlers(*newcomm,
                                          oxton::peruse_event_handler);
}

extern "C" int MPI_Comm_free(MPI_Comm *comm)
{
    MPI_Comm cm = *comm;

    int ret = PMPI_Comm_free(comm);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    oxton::unregister_comm(*comm);

    return oxton::remove_event_handlers(*comm);
}

extern "C" int MPI_Finalize()
{
    for (const auto& comm : oxton::comms) {
        oxton::remove_event_handlers(comm);
    }

    int rank;
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::stringstream path;
    path << "oxton-result" << rank << ".json";

    oxton::trace.write_result(path.str());

    return PMPI_Finalize();
}
