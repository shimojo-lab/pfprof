#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <mpi.h>
extern "C" {
#include <peruse.h>
};

#include "trace_analyzer.hpp"

#define NUM_REQ_EVENT_NAMES (2)

namespace oxton {

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

static trace_analyzer analyzer;

int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
                         peruse_comm_spec_t *spec, void *param)
{
    int ev_type, sz;
    PMPI_Type_size(spec->datatype, &sz);
    int  len = spec->count * sz;

    int peer = lg_rank_table[spec->comm][spec->peer];

    PERUSE_Event_get(event_handle, &ev_type);

    switch (ev_type) {
    case PERUSE_COMM_REQ_XFER_BEGIN:
        if (spec->operation == PERUSE_SEND) {
            oxton::analyzer.feed_event(EV_BEGIN_SEND, peer, len, spec->tag);
        } else if (spec->operation == PERUSE_RECV) {
            oxton::analyzer.feed_event(EV_BEGIN_RECV, peer, len, spec->tag);
        } else {
            std::cout << "Unexpected operation type\n" << std::endl;
            return MPI_ERR_INTERN;
        }
        break;

    case PERUSE_COMM_REQ_XFER_END:
        if (spec->operation == PERUSE_SEND) {
            oxton::analyzer.feed_event(EV_END_SEND, peer, len, spec->tag);
        } else if (spec->operation == PERUSE_RECV) {
            oxton::analyzer.feed_event(EV_END_RECV, peer, len, spec->tag);
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
    int sz;
    MPI_Group group, world_group;

    PMPI_Comm_group(comm, &group);
    PMPI_Comm_group(MPI_COMM_WORLD, &world_group);
    PMPI_Comm_size(comm, &sz);

    std::vector<int> ranks(sz), world_ranks(sz);

    for (int i = 0; i < sz; i++) {
        ranks[i] = i;
    }

    PMPI_Group_translate_ranks(group, sz, ranks.data(), world_group,
                               world_ranks.data());

    for (int i = 0; i < sz; i++) {
        lg_rank_table[comm].push_back(world_ranks[i]);
    }

    return EXIT_SUCCESS;
}

static int initialize()
{
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int processor_name_len;
    PMPI_Get_processor_name(processor_name, &processor_name_len);
    int rank;
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int n_procs;
    PMPI_Comm_size(MPI_COMM_WORLD, &n_procs);

    oxton::analyzer.set_processor_name(processor_name);
    oxton::analyzer.set_rank(rank);
    oxton::analyzer.set_description("Generated by Oxton v0.2.0");
    oxton::analyzer.set_n_procs(n_procs);

    // Initialize PERUSE
    int ret = PERUSE_Init();
    if (ret != PERUSE_SUCCESS) {
        std::cout << "Unable to initialize PERUSE" << std::endl;
        return MPI_ERR_INTERN;
    }

    // Query PERUSE to see if the events of interest are supported
    for (const auto& req_event : oxton::req_events) {
        oxton::event_desc_t desc;

        ret = PERUSE_Query_event(req_event, &desc);
        if (ret != PERUSE_SUCCESS) {
            std::cout << "Event " << req_event << " not supported"
                      << std::endl;
            return MPI_ERR_INTERN;
        }

        oxton::ev_table[desc] = std::unordered_map<MPI_Comm, peruse_event_h>();
    }

    oxton::register_comm(MPI_COMM_WORLD);
    oxton::register_comm(MPI_COMM_SELF);

    return oxton::register_event_handlers(MPI_COMM_WORLD,
                                          oxton::peruse_event_handler);
}

}

// MPI functions

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

    oxton::analyzer.write_result(path.str());

    return PMPI_Finalize();
}

// Wrapper functions for FORTRAN

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
