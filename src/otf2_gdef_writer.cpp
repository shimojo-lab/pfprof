#include <otf2/otf2.h>
#include <stdio.h>
#include <stdlib.h>

#include "otf2_writer.h"

enum {
    G_STR_EMPTY,
    G_STR_MASTER_THREAD,
    G_STR_MPI,
    G_STR_COMM_WORLD,
    G_STR_PROCESS,
};

static int write_location_global_defs(OTF2_GlobalDefWriter *w)
{
    OTF2_GlobalDefWriter_WriteString(w, G_STR_EMPTY, "");
    OTF2_GlobalDefWriter_WriteString(w, G_STR_MASTER_THREAD, "Master Thread");
    OTF2_GlobalDefWriter_WriteString(w, G_STR_MPI, "MPI");
    OTF2_GlobalDefWriter_WriteString(w, G_STR_COMM_WORLD, "MPI_COMM_WORLD");

    for (int rank = 0; rank < num_procs; rank++) {
        char process_name[32];

        sprintf(process_name, "MPI Rank %d", rank);
        OTF2_GlobalDefWriter_WriteString(w, G_STR_PROCESS + rank, process_name);

        /* Process */
        OTF2_GlobalDefWriter_WriteLocationGroup(
            w, rank /* id == rank */, G_STR_PROCESS + rank /* name */,
            OTF2_LOCATION_GROUP_TYPE_PROCESS, OTF2_UNDEFINED_SYSTEM_TREE_NODE);

        /* Thread */
        OTF2_GlobalDefWriter_WriteLocation(
            w, rank /* id */, 1 /* name */, OTF2_LOCATION_TYPE_CPU_THREAD,
            100 /* number of events */, rank /* proces */);
    }

    return EXIT_SUCCESS;
}

static int write_communicator_global_defs(OTF2_GlobalDefWriter *w)
{
    uint64_t *comm_locations;
    comm_locations = (uint64_t *)calloc(num_procs, sizeof(*comm_locations));
    for (int rank = 0; rank < num_procs; rank++) {
        comm_locations[rank] = rank;
    }

    /* MPI Group */
    OTF2_GlobalDefWriter_WriteGroup(
        w, 0 /* id */, G_STR_MPI /* name */, OTF2_GROUP_TYPE_COMM_LOCATIONS,
        OTF2_PARADIGM_MPI, OTF2_GROUP_FLAG_NONE, num_procs, comm_locations);

    /* MPI_COMM_WORLD Group */
    OTF2_GlobalDefWriter_WriteGroup(
        w, 1 /* id */, G_STR_EMPTY /* name */, OTF2_GROUP_TYPE_COMM_GROUP,
        OTF2_PARADIGM_MPI, OTF2_GROUP_FLAG_NONE, num_procs, comm_locations);

    /* MPI_COMM_WORLD */
    OTF2_GlobalDefWriter_WriteComm(w, 0 /* id */, G_STR_COMM_WORLD /* name */,
                                   1 /* group */, OTF2_UNDEFINED_COMM);

    free(comm_locations);

    return EXIT_SUCCESS;
}

int write_global_defs()
{
    OTF2_GlobalDefWriter *writer;

    writer = OTF2_Archive_GetGlobalDefWriter(archive);

    uint64_t global_epoch_len = global_epoch_end - global_epoch_start + 1;
    OTF2_GlobalDefWriter_WriteClockProperties(
        writer, 1000000000, global_epoch_start, global_epoch_len);

    write_location_global_defs(writer);
    write_communicator_global_defs(writer);

    OTF2_Archive_CloseGlobalDefWriter(archive, writer);

    return EXIT_SUCCESS;
}
