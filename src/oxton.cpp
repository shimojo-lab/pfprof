#include <iostream>
#include <vector>
#include <unordered_map>
#include <utility>

#include <mpi.h>
extern "C" {
#include <peruse.h>
};

#include "oxton.h"
#include "otf2_writer.h"

#define NUM_REQ_EVENT_NAMES (2)

/* Events for the occurrence of which the profiler will be notified */
static const char *req_events[NUM_REQ_EVENT_NAMES] = {
    "PERUSE_COMM_REQ_XFER_BEGIN",
    "PERUSE_COMM_REQ_XFER_END",
};

int num_procs, my_rank;

static int register_event_handlers(MPI_Comm comm,
        peruse_comm_callback_f *callback);
static int remove_event_handlers(MPI_Comm comm);
static int register_comm(MPI_Comm comm);

typedef int peruse_event_desc;
typedef std::unordered_map<MPI_Comm, peruse_event_h> hoge;
typedef std::unordered_map<peruse_event_desc, hoge> eh_table_t;

static std::vector<MPI_Comm> comms;
static std::unordered_map<MPI_Aint, uint64_t> req_id_table;
static int max_req_id;
static std::unordered_map<MPI_Comm, std::vector<int> > lg_rank_table;
static eh_table_t eh_table;

int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
        peruse_comm_spec_t *spec, void *param)
{
    int ev_type, sz, len;
    uint64_t req_id = (uint64_t)unique_id;
    uint64_t dst;

    /* Ignore all recv events and send events to myself */
    dst = lg_rank_table[spec->comm][spec->peer];
    if(dst == my_rank || spec->operation != PERUSE_SEND) {
        return MPI_SUCCESS;
    }

    PERUSE_Event_get(event_handle, &ev_type);
    switch(ev_type) {
        case PERUSE_COMM_REQ_XFER_BEGIN:
            MPI_Type_size(spec->datatype, &sz);
            len = spec->count * sz;
            req_id_table[unique_id] = max_req_id++;
            write_xfer_begin_event(dst, spec->tag, len, max_req_id);
            break;

        case PERUSE_COMM_REQ_XFER_END:
            write_xfer_end_event(req_id_table[unique_id]);
            req_id_table.erase(unique_id);
            break;

        default:
            std::cout << "Unexpected event in callback\n" << std::endl;
            return MPI_ERR_INTERN;
    }

    return MPI_SUCCESS;
}

extern "C" int MPI_Init(int *argc, char ***argv)
{
    int ret;

    PMPI_Init(argc, argv);
    PMPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    open_otf2_writer();

    /* Initialize PERUSE */
    ret = PERUSE_Init();
    if(ret != PERUSE_SUCCESS) {
        std::cout << "Unable to initialize PERUSE" << std::endl;
        return MPI_ERR_INTERN;
    }

    /* Query PERUSE to see if the events of interest are supported */
    for(int i = 0; i < NUM_REQ_EVENT_NAMES; i++) {
        peruse_event_desc desc;

        ret = PERUSE_Query_event(req_events[i], &desc);
        if(ret != PERUSE_SUCCESS) {
            std::cout << "Events " << req_events[i] << " not supported" << std::endl;
            return MPI_ERR_INTERN;
        }

        eh_table[desc] = std::unordered_map<MPI_Comm, peruse_event_h>();
    }

    register_comm(MPI_COMM_WORLD);
    register_comm(MPI_COMM_SELF);

    return register_event_handlers(MPI_COMM_WORLD, peruse_event_handler);
}

extern "C" int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_create(comm, group, newcomm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    register_comm(*newcomm);

    return register_event_handlers(*newcomm, peruse_event_handler);
}

extern "C" int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_dup(comm, newcomm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    register_comm(*newcomm);

    return register_event_handlers(*newcomm, peruse_event_handler);
}

extern "C" int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_split(comm, color, key, newcomm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    register_comm(*newcomm);

    return register_event_handlers(*newcomm, peruse_event_handler);
}

extern "C" int MPI_Comm_free(MPI_Comm *comm)
{
    int comm_idx, ret;
    MPI_Comm cm = *comm;

    ret = PMPI_Comm_free(comm);
    if(ret != MPI_SUCCESS) {
        return ret;
    }

    return remove_event_handlers(*comm);
}

extern "C" int MPI_Finalize()
{
    /* Deactivate event handles and free them for all comms */
    for(int i = 0; i < comms.size(); i++) {
        remove_event_handlers(comms[i]);
    }

    close_otf2_writer();

    return PMPI_Finalize();
}

int register_event_handlers(MPI_Comm comm, peruse_comm_callback_f *callback)
{
    /* Initialize event handles with comm and activate them */
    comms.push_back(comm);

    eh_table_t::iterator it;
    for (it = eh_table.begin(); it != eh_table.end(); ++it) {
        peruse_event_h eh;

        PERUSE_Event_comm_register(it->first, comm, callback, NULL, &eh);
        it->second[comm] = eh;
        PERUSE_Event_activate(eh);
    }

    return MPI_SUCCESS;
}

int remove_event_handlers(MPI_Comm comm)
{
    eh_table_t::iterator it;
    for (it = eh_table.begin(); it != eh_table.end(); ++it) {
        PERUSE_Event_deactivate(it->second[comm]);
        PERUSE_Event_release(&it->second[comm]);
    }

    return MPI_SUCCESS;
}

int register_comm(MPI_Comm comm)
{
    int sz, *ranks, *world_ranks;
    MPI_Group group;
    MPI_Group world_group;

    MPI_Comm_group(comm, &group);
    MPI_Comm_group(MPI_COMM_WORLD, &world_group);
    MPI_Comm_size(comm, &sz);

    ranks = (int *)calloc(sz, sizeof(*ranks));
    world_ranks = (int *)calloc(sz, sizeof(*world_ranks));
    for (int i = 0; i < sz; i++) {
        ranks[i] = i;
    }

    MPI_Group_translate_ranks(group, sz, ranks, world_group, world_ranks);

    for (int i = 0; i < sz; i++) {
        lg_rank_table[comm].push_back(world_ranks[i]);
    }

    return EXIT_SUCCESS;
}
