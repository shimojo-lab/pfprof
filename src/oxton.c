#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <peruse.h>

#include "uthash.h"
#include "oxton.h"
#include "otf2_writer.h"

#define NUM_REQ_EVENT_NAMES (2)

/* Events for the occurrence of which the profiler will be notified */
char *req_events[NUM_REQ_EVENT_NAMES] = {
    "PERUSE_COMM_REQ_XFER_BEGIN",
    "PERUSE_COMM_REQ_XFER_END",
};

struct event_handle_table
{
    int descriptor;
    peruse_event_h *handles;
};

int num_procs, my_rank;

static int register_event_handlers(MPI_Comm comm,
        peruse_comm_callback_f *callback);
static int remove_event_handlers(int comm_idx);

static int num_comms = 0;
static MPI_Comm *comms = NULL;
static struct event_handle_table event_table[NUM_REQ_EVENT_NAMES];

/* Callback for collecting statistics */
int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
        peruse_comm_spec_t *spec, void *param)
{
    int ev_type, sz, len;
    uint64_t req_id = (uint64_t)unique_id;

    /* Ignore all recv events and send events to myself */
    if(spec->peer == my_rank || spec->operation == PERUSE_RECV) {
        return MPI_SUCCESS;
    }

    PERUSE_Event_get(event_handle, &ev_type);
    switch(ev_type) {
        case PERUSE_COMM_REQ_XFER_BEGIN:
            MPI_Type_size(spec->datatype, &sz);
            len = spec->count * sz;
            printf("%d\n", spec->tag);
            write_xfer_begin_event(spec->peer, spec->tag, len, req_id);
            break;

        case PERUSE_COMM_REQ_XFER_END:
            write_xfer_end_event(req_id);
            break;

        default:
            printf("Unexpected event in callback\n");
            return MPI_ERR_INTERN;
    }

    return MPI_SUCCESS;
}

/* Profiler functions */
int MPI_Init(int *argc, char ***argv)
{
    int i, ret, descriptor;
    peruse_event_h eh;

    PMPI_Init(argc, argv);
    PMPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    open_otf2_writer();

    /* Initialize PERUSE */
    ret = PERUSE_Init();
    if(ret != PERUSE_SUCCESS) {
        printf("Unable to initialize PERUSE\n");
        return MPI_ERR_INTERN;
    }

    /* Query PERUSE to see if the events of interest are supported */
    for(i = 0; i < NUM_REQ_EVENT_NAMES; i++) {
        ret = PERUSE_Query_event(req_events[i], &event_table[i].descriptor);
        if(ret != PERUSE_SUCCESS) {
            printf("Event %s not supported\n", req_events[i]);
            return MPI_ERR_INTERN;
        }
    }

    return register_event_handlers(MPI_COMM_WORLD, peruse_event_handler);
}

int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_create(comm, group, newcomm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    return register_event_handlers(*newcomm, peruse_event_handler);
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_dup(comm, newcomm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    return register_event_handlers(*newcomm, peruse_event_handler);
}

int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_split(comm, color, key, newcomm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    return register_event_handlers(*newcomm, peruse_event_handler);
}

int MPI_Comm_free(MPI_Comm *comm)
{
    int i, comm_idx, ret;
    MPI_Comm cm = *comm;

    ret = PMPI_Comm_free(comm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    for(i = 0; i < num_comms; i++) {
        if(cm == comms[i]) {
            comm_idx = i;
        }
    }

    return remove_event_handlers(comm_idx);
}

int MPI_Finalize()
{
    int i;

    close_otf2_writer();

    /* Deactivate event handles and free them for all comms */
    for(i = 0; i < num_comms; i++) {
        remove_event_handlers(i);
    }

    /* Free event handle array */
    for(i = 0; i < NUM_REQ_EVENT_NAMES; i++) {
        free(event_table[i].handles);
    }

    free(comms);

    return PMPI_Finalize();
}

/* Support functions */
int register_event_handlers(MPI_Comm comm, peruse_comm_callback_f *callback)
{
    int i;
    peruse_event_h eh;

    /* Initialize event handles with comm and activate them */
    num_comms++;
    comms = (MPI_Comm *)realloc(comms, num_comms * sizeof(MPI_Comm));
    comms[num_comms - 1] = comm;

    for(i = 0; i < NUM_REQ_EVENT_NAMES; i++) {
        PERUSE_Event_comm_register(event_table[i].descriptor, comm, callback,
                NULL, &eh);
        event_table[i].handles = (peruse_event_h *)realloc(
                event_table[i].handles, num_comms * sizeof(peruse_event_h));
        event_table[i].handles[num_comms - 1] = eh;
        PERUSE_Event_activate(eh);
    }

    return MPI_SUCCESS;
}

int remove_event_handlers(int comm_idx)
{
    int i;

    for(i = 0; i < NUM_REQ_EVENT_NAMES; i++) {
        PERUSE_Event_deactivate(event_table[i].handles[comm_idx]);
        PERUSE_Event_release(&event_table[i].handles[comm_idx]);
    }

    return MPI_SUCCESS;
}
