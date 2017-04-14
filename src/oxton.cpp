#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <mpi.h>
extern "C" {
#include <peruse.h>
};

#define NUM_REQ_EVENT_NAMES (2)

// Events for the occurrence of which the profiler will be notified
static const char *req_events[NUM_REQ_EVENT_NAMES] = {
    "PERUSE_COMM_REQ_XFER_BEGIN", "PERUSE_COMM_REQ_XFER_END",
};

// Register event handlers for a communicator
static int register_event_handlers(MPI_Comm comm,
                                   peruse_comm_callback_f *callback);
// Remove a communicator from table
static int remove_event_handlers(MPI_Comm comm);
// Register a communicator to table
static int register_comm(MPI_Comm comm);

// PERUSE event descriptor
typedef int event_desc_t;
// Mapping from communicators to PERUSE event handlers
typedef std::unordered_map<MPI_Comm, peruse_event_h> eh_table_t;
// Mapping from PERUSE event descriptors to event handler tables
typedef std::unordered_map<event_desc_t, eh_table_t> ev_table_t;

// List of communicators
static std::vector<MPI_Comm> comms;
// Mapping from communicator to local-global rank table
static std::unordered_map<MPI_Comm, std::vector<int>> lg_rank_table;
static ev_table_t ev_table;

int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
                         peruse_comm_spec_t *spec, void *param)
{
    int ev_type, sz, len;
    uint64_t peer = lg_rank_table[spec->comm][spec->peer];

    MPI_Type_size(spec->datatype, &sz);
    len = spec->count * sz;

    PERUSE_Event_get(event_handle, &ev_type);

    switch (ev_type) {
    case PERUSE_COMM_REQ_XFER_BEGIN:
        if (spec->operation == PERUSE_SEND) {
        } else if (spec->operation == PERUSE_RECV) {
        } else {
            std::cout << "Unexpected operation type\n" << std::endl;
            return MPI_ERR_INTERN;
        }
        break;

    case PERUSE_COMM_REQ_XFER_END:
        MPI_Type_size(spec->datatype, &sz);
        len = spec->count * sz;

        if (spec->operation == PERUSE_SEND) {
        } else if (spec->operation == PERUSE_RECV) {
        } else {
            std::cout << "Unexpected operation type\n" << std::endl;
            return MPI_ERR_INTERN;
        }
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
    int num_procs, my_rank;

    PMPI_Init(argc, argv);
    PMPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    PMPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // Initialize PERUSE
    ret = PERUSE_Init();
    if (ret != PERUSE_SUCCESS) {
        std::cout << "Unable to initialize PERUSE" << std::endl;
        return MPI_ERR_INTERN;
    }

    // Query PERUSE to see if the events of interest are supported
    for (const auto& req_event : req_events) {
        event_desc_t desc;

        ret = PERUSE_Query_event(req_event, &desc);
        if (ret != PERUSE_SUCCESS) {
            std::cout << "Events " << req_events << " not supported"
                      << std::endl;
            return MPI_ERR_INTERN;
        }

        ev_table[desc] = std::unordered_map<MPI_Comm, peruse_event_h>();
    }

    register_comm(MPI_COMM_WORLD);
    register_comm(MPI_COMM_SELF);

    return register_event_handlers(MPI_COMM_WORLD, peruse_event_handler);
}

extern "C" int MPI_Comm_create(MPI_Comm comm, MPI_Group group,
                               MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_create(comm, group, newcomm);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    register_comm(*newcomm);

    return register_event_handlers(*newcomm, peruse_event_handler);
}

extern "C" int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_dup(comm, newcomm);
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    register_comm(*newcomm);

    return register_event_handlers(*newcomm, peruse_event_handler);
}

extern "C" int MPI_Comm_split(MPI_Comm comm, int color, int key,
                              MPI_Comm *newcomm)
{
    int ret;

    ret = PMPI_Comm_split(comm, color, key, newcomm);
    if (ret != MPI_SUCCESS) {
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
    if (ret != MPI_SUCCESS) {
        return ret;
    }

    return remove_event_handlers(*comm);
}

extern "C" int MPI_Finalize()
{
    for (const auto& comm : comms) {
        remove_event_handlers(comm);
    }

    return PMPI_Finalize();
}

int register_event_handlers(MPI_Comm comm, peruse_comm_callback_f *callback)
{
    comms.push_back(comm);

    for (auto& kv : ev_table) {
        peruse_event_h eh;

        PERUSE_Event_comm_register(kv.first, comm, callback, NULL, &eh);
        kv.second[comm] = eh;
        PERUSE_Event_activate(eh);
    }

    return MPI_SUCCESS;
}

int remove_event_handlers(MPI_Comm comm)
{
    for (auto& kv : ev_table) {
        PERUSE_Event_deactivate(kv.second[comm]);
        PERUSE_Event_release(&kv.second[comm]);
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

// Wrapper functions for FORTRAN

extern "C" void mpi_finalize_(MPI_Fint *ierr)
{
    int c_ierr = MPI_Finalize();
    if (NULL != ierr) *ierr = c_ierr;
}

extern "C" void mpi_init_(MPI_Fint *ierr)
{
    int c_ierr;
    int argc = 0;
    char **argv = NULL;

    c_ierr = MPI_Init(&argc, &argv);
    if (NULL != ierr) *ierr = c_ierr;
}

extern "C" void mpi_comm_create_f(MPI_Fint *comm, MPI_Fint *group,
                                  MPI_Fint *newcomm, MPI_Fint *ierr)
{
    int c_ierr;
    MPI_Comm c_newcomm;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);
    MPI_Group c_group = PMPI_Group_f2c(*group);

    c_ierr = MPI_Comm_create(c_comm, c_group, &c_newcomm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *newcomm = PMPI_Comm_c2f(c_newcomm);
    }
}

extern "C" void mpi_comm_dup_(MPI_Fint *comm, MPI_Fint *newcomm, MPI_Fint *ierr)
{
    int c_ierr;
    MPI_Comm c_newcomm;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);

    c_ierr = MPI_Comm_dup(c_comm, &c_newcomm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *newcomm = PMPI_Comm_c2f(c_newcomm);
    }
}

extern "C" void mpi_comm_split_(MPI_Fint *comm, MPI_Fint *color, MPI_Fint *key,
                                MPI_Fint *newcomm, MPI_Fint *ierr)
{
    int c_ierr;
    MPI_Comm c_newcomm;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);

    c_ierr = MPI_Comm_split(c_comm, *color, *key, &c_newcomm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *newcomm = PMPI_Comm_c2f(c_newcomm);
    }
}

extern "C" void mpi_comm_free_(MPI_Fint *comm, MPI_Fint *ierr)
{
    int c_ierr;
    MPI_Comm c_comm = PMPI_Comm_f2c(*comm);

    c_ierr = MPI_Comm_free(&c_comm);
    if (NULL != ierr) *ierr = c_ierr;

    if (MPI_SUCCESS == c_ierr) {
        *comm = PMPI_Comm_c2f(c_comm);
    }
}
