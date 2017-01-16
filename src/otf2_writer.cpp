#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <otf2/otf2.h>
#include <otf2/OTF2_MPI_Collectives.h>

#include "oxton.h"
#include "otf2_writer.h"

OTF2_Archive *archive;
uint64_t epoch_start;
uint64_t epoch_end;
uint64_t global_epoch_start;
uint64_t global_epoch_end;

static OTF2_TimeStamp get_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)(ts.tv_sec * 1000000000 + ts.tv_nsec);
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

static OTF2_EvtWriter* evt_writer;
static OTF2_DefWriter *def_writer;

int open_otf2_writer()
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

    def_writer = OTF2_Archive_GetDefWriter(archive, my_rank);

    epoch_start = get_time();

    return EXIT_SUCCESS;
}

int close_otf2_writer()
{
    epoch_end = get_time();

    OTF2_Archive_CloseDefWriter(archive, def_writer);

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

    OTF2_Archive_Close(archive);

    return EXIT_SUCCESS;
}

int write_xfer_begin_event(uint32_t dst, uint32_t tag, uint32_t len,
                            uint64_t req_id)
{
    OTF2_EvtWriter_MpiIsend(evt_writer, NULL, get_time(), dst, 0, tag, len,
                                req_id);

    return EXIT_SUCCESS;
}

int write_xfer_end_event(uint64_t req_id)
{
    OTF2_EvtWriter_MpiIsendComplete(evt_writer, NULL, get_time(), req_id);

    return EXIT_SUCCESS;
}
