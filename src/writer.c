#include <stdio.h>
#include <stdlib.h>
#include <otf2/otf2.h>
#include <otf2/OTF2_MPI_Collectives.h>

#include "oxton.h"

static OTF2_TimeStamp get_time()
{
    double t = MPI_Wtime() * 1e9;
    return (uint64_t)t;
}

static OTF2_FlushType pre_flush(void *userData, OTF2_FileType fileType,
        OTF2_LocationRef location, void *callerData, bool final)
{
    /* Flush recorded data to file */
    return OTF2_FLUSH;
}

static OTF2_TimeStamp post_flush(void *userData, OTF2_FileType fileType,
        OTF2_LocationRef location)
{
    return get_time();
}

/* Register pre/post flush callbacks
 * Flush happens when internal buffer in OTF2 is full */
static OTF2_FlushCallbacks flush_callbacks = {
    .otf2_pre_flush  = pre_flush,
    .otf2_post_flush = post_flush
};

static OTF2_Archive *archive;
static OTF2_EvtWriter* evt_writer;
static uint64_t epoch_start;
static uint64_t epoch_end;
static uint64_t global_epoch_start;
static uint64_t global_epoch_end;

static void write_global_defs();

void init_writer()
{
    archive = OTF2_Archive_Open("trace", "oxton-test", OTF2_FILEMODE_WRITE,
            1024 * 1024 /* event chunk size */,
            4 * 1024 * 1024 /* def chunk size */,
            OTF2_SUBSTRATE_POSIX, OTF2_COMPRESSION_NONE);

    OTF2_Archive_SetFlushCallbacks(archive, &flush_callbacks, NULL);
    OTF2_MPI_Archive_SetCollectiveCallbacks(archive,
            MPI_COMM_WORLD, MPI_COMM_NULL);

    OTF2_Archive_SetDescription(archive, "MPI trace log captured with Oxton");
    OTF2_Archive_SetCreator(archive, "Oxton v0.0.1");

    /* Open event file and get writer */
    OTF2_Archive_OpenEvtFiles(archive);
    evt_writer = OTF2_Archive_GetEvtWriter(archive, my_rank);

    epoch_start = get_time();
}

void close_writer()
{
    epoch_end = get_time();

    OTF2_Archive_CloseEvtWriter(archive, evt_writer);
    OTF2_Archive_CloseEvtFiles(archive);

    uint64_t global_epoch_start;
    MPI_Reduce(&epoch_start, &global_epoch_start, 1, OTF2_MPI_UINT64_T,
            MPI_MIN, 0, MPI_COMM_WORLD);

    uint64_t global_epoch_end;
    MPI_Reduce(&epoch_end, &global_epoch_end, 1, OTF2_MPI_UINT64_T,
            MPI_MAX, 0, MPI_COMM_WORLD);

    if (my_rank == 0) {
        write_global_defs();
    }

    /* Create empty local defs to supress warnings */
    OTF2_DefWriter *local_def_writer =
        OTF2_Archive_GetDefWriter(archive, my_rank);
    OTF2_Archive_CloseDefWriter(archive, local_def_writer);

    OTF2_Archive_Close(archive);
}

enum {
    G_STR_EMPTY,
    G_STR_MASTER_THREAD,
    G_STR_MPI,
    G_STR_MPI_COMM_WORLD,
    G_STR_PROCESS,
};

void write_global_defs()
{
    OTF2_GlobalDefWriter *writer = NULL;
    uint64_t *comm_locations = NULL;

    writer = OTF2_Archive_GetGlobalDefWriter(archive);

    OTF2_GlobalDefWriter_WriteClockProperties(writer,
        1000000000, global_epoch_start, global_epoch_end - global_epoch_start + 1 );

    OTF2_GlobalDefWriter_WriteString(writer, G_STR_EMPTY, "");
    OTF2_GlobalDefWriter_WriteString(writer, G_STR_MASTER_THREAD, "Master Thread");
    OTF2_GlobalDefWriter_WriteString(writer, G_STR_MPI, "MPI");
    OTF2_GlobalDefWriter_WriteString(writer, G_STR_MPI_COMM_WORLD, "MPI_COMM_WORLD");

    for (int rank = 0; rank < num_procs; rank++) {
        char process_name[32];

        sprintf(process_name, "MPI Rank %d", rank);
        OTF2_GlobalDefWriter_WriteString(writer, G_STR_PROCESS + rank,
                process_name);

        /* Process */
        OTF2_GlobalDefWriter_WriteLocationGroup(writer,
                rank /* id == rank */, G_STR_PROCESS + rank /* name */,
                OTF2_LOCATION_GROUP_TYPE_PROCESS,
                OTF2_UNDEFINED_SYSTEM_TREE_NODE);

        /* Thread */
        OTF2_GlobalDefWriter_WriteLocation(
                writer, rank /* id */, 1 /* name */,
                OTF2_LOCATION_TYPE_CPU_THREAD, 100 /* number of events */,
                rank /* proces */);
    }

    comm_locations = (uint64_t *)calloc(num_procs, sizeof(uint64_t));
    for (int rank = 0; rank < num_procs; rank++) {
        comm_locations[rank] = rank;
    }

    /* MPI Group */
    OTF2_GlobalDefWriter_WriteGroup(writer,
        0 /* id */, G_STR_MPI /* name */, OTF2_GROUP_TYPE_COMM_LOCATIONS,
        OTF2_PARADIGM_MPI, OTF2_GROUP_FLAG_NONE, num_procs, comm_locations);

    /* MPI_COMM_WORLD Group */
    OTF2_GlobalDefWriter_WriteGroup(writer,
        1 /* id */, G_STR_EMPTY /* name */, OTF2_GROUP_TYPE_COMM_GROUP,
        OTF2_PARADIGM_MPI, OTF2_GROUP_FLAG_NONE, num_procs, comm_locations);

    /* MPI_COMM_WORLD */
    OTF2_GlobalDefWriter_WriteComm(writer,
        0 /* id */, G_STR_MPI_COMM_WORLD /* name */, 1 /* group */,
        OTF2_UNDEFINED_COMM);

    free(comm_locations);

    OTF2_Archive_CloseGlobalDefWriter(archive, writer);
}

void write_xfer_event(xfer_event_t *ev)
{
    printf("xfer from %d to %d in %f[s]\n", ev->src, ev->dst,
            ev->end_time - ev->start_time);

    OTF2_EvtWriter_MpiSend(evt_writer, NULL, get_time(), ev->dst, 0, 0, ev->size);
}
