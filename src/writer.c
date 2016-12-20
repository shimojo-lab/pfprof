#include <stdio.h>
#include <stdlib.h>
#include <otf2/otf2.h>
#include <otf2/OTF2_MPI_Collectives.h>

#include "oxton.h"

static OTF2_TimeStamp get_time()
{
    double t = MPI_Wtime() * 1e9;
    return ( uint64_t )t;
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

void init_writer()
{
    archive = OTF2_Archive_Open("trace", "oxton-test", OTF2_FILEMODE_WRITE,
            1024 * 1024 /* event chunk size */,
            4 * 1024 * 1024 /* def chunk size */,
            OTF2_SUBSTRATE_POSIX, OTF2_COMPRESSION_NONE);

    OTF2_Archive_SetFlushCallbacks(archive, &flush_callbacks, NULL);
    OTF2_MPI_Archive_SetCollectiveCallbacks(archive,
            MPI_COMM_WORLD, MPI_COMM_NULL);

    /* Open event file and get writer */
    OTF2_Archive_OpenEvtFiles(archive);
    evt_writer = OTF2_Archive_GetEvtWriter(archive, my_rank);
    get_time();
}

void close_writer()
{
    OTF2_Archive_CloseEvtWriter(archive, evt_writer);
    OTF2_Archive_CloseEvtFiles(archive);

    uint64_t global_epoch_start;
    MPI_Reduce(&epoch_start, &global_epoch_start, 1, OTF2_MPI_UINT64_T,
            MPI_MIN, 0, MPI_COMM_WORLD);

    uint64_t global_epoch_end;
    MPI_Reduce(&epoch_end, &global_epoch_end, 1, OTF2_MPI_UINT64_T,
            MPI_MAX, 0, MPI_COMM_WORLD);

    if (my_rank == 0) {
        OTF2_GlobalDefWriter* global_def_writer = OTF2_Archive_GetGlobalDefWriter(archive);

        OTF2_GlobalDefWriter_WriteClockProperties(global_def_writer,
                                                  1000000000,
                                                  global_epoch_start,
                                                  global_epoch_end - global_epoch_start + 1 );

        OTF2_GlobalDefWriter_WriteString(global_def_writer, 0, "" );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, 1, "Master Thread" );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, 4, "barrier" );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, 5, "MyHost" );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, 6, "node" );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, 7, "MPI" );
        OTF2_GlobalDefWriter_WriteString(global_def_writer, 8, "MPI_COMM_WORLD" );
        OTF2_GlobalDefWriter_WriteSystemTreeNode( global_def_writer,
                                                  0 /* id */,
                                                  5 /* name */,
                                                  6 /* class */,
                                                  OTF2_UNDEFINED_SYSTEM_TREE_NODE /* parent */ );

        for ( int r = 0; r < num_procs; r++ ) {
            char process_name[ 32 ];
            sprintf( process_name, "MPI Rank %d", r );
            OTF2_GlobalDefWriter_WriteString( global_def_writer,
                                              9 + r,
                                              process_name );
            OTF2_GlobalDefWriter_WriteLocationGroup(global_def_writer,
                                                    r /* id */,
                                                    9 + r /* name */,
                                                    OTF2_LOCATION_GROUP_TYPE_PROCESS,
                                                    0 /* system tree */ );
            OTF2_GlobalDefWriter_WriteLocation(global_def_writer,
                                               r /* id */,
                                               1 /* name */,
                                               OTF2_LOCATION_TYPE_CPU_THREAD,
                                               4 /* # events */,
                                               r /* location group */ );

            uint64_t *comm_locations = (uint64_t *)calloc(num_procs, sizeof(uint64_t));
            comm_locations[r] = r;
        }

        OTF2_GlobalDefWriter_WriteGroup( global_def_writer,
                                         0 /* id */,
                                         7 /* name */,
                                         OTF2_GROUP_TYPE_COMM_LOCATIONS,
                                         OTF2_PARADIGM_MPI,
                                         OTF2_GROUP_FLAG_NONE,
                                         num_procs,
                                         comm_locations );
        OTF2_GlobalDefWriter_WriteGroup( global_def_writer,
                                         1 /* id */,
                                         0 /* name */,
                                         OTF2_GROUP_TYPE_COMM_GROUP,
                                         OTF2_PARADIGM_MPI,
                                         OTF2_GROUP_FLAG_NONE,
                                         num_procs,
                                         comm_locations );
        OTF2_GlobalDefWriter_WriteComm( global_def_writer,
                                        0 /* id */,
                                        8 /* name */,
                                        1 /* group */,
                                        OTF2_UNDEFINED_COMM /* parent */ );

        free(comm_locations);

        OTF2_Archive_CloseGlobalDefWriter(archive, global_def_writer);
    }

    /* Create empty local def to supress warnings */
    OTF2_DefWriter *local_def_writer = OTF2_Archive_GetDefWriter(archive, my_rank);
    OTF2_Archive_CloseDefWriter(archive, local_def_writer);

    OTF2_Archive_Close(archive);
}

void write_xfer_event(xfer_event_t *ev)
{
    printf("xfer from %d to %d in %f[s]\n", ev->src, ev->dst,
            ev->end_time - ev->start_time);

    OTF2_EvtWriter_MpiSend(evt_writer, NULL, get_time(), ev->dst, 0, 0, ev->size);
}
