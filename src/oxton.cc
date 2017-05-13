#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" {
#include <mpi.h>
#include <peruse.h>
};

#include "oxton.hpp"
#include "trace_analyzer.hpp"

namespace oxton {

static const char *req_events[NUM_REQ_EVENT_NAMES] = {
    "PERUSE_COMM_REQ_ACTIVATE", "PERUSE_COMM_REQ_COMPLETE",
};

std::unordered_set<MPI_Comm> comms;
std::unordered_map<MPI_Comm, std::vector<int>> lg_rank_table;
ev_table_t ev_table;
trace_analyzer analyzer;

int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
                         peruse_comm_spec_t *spec, void *param)
{
    int ev_type, sz;
    PMPI_Type_size(spec->datatype, &sz);
    int  len = spec->count * sz;

    int peer = lg_rank_table[spec->comm][spec->peer];

    PERUSE_Event_get(event_handle, &ev_type);

    switch (ev_type) {
    case PERUSE_COMM_REQ_ACTIVATE:
        if (spec->operation == PERUSE_SEND) {
            oxton::analyzer.feed_event(EV_BEGIN_SEND, peer, len, spec->tag);
        } else if (spec->operation == PERUSE_RECV) {
            oxton::analyzer.feed_event(EV_BEGIN_RECV, peer, len, spec->tag);
        } else {
            std::cout << "Unexpected operation type\n" << std::endl;
            return MPI_ERR_INTERN;
        }
        break;

    case PERUSE_COMM_REQ_COMPLETE:
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
    comms.insert(comm);

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

int unregister_comm(MPI_Comm comm)
{
    oxton::comms.erase(comm);

    lg_rank_table.erase(comm);

    for (auto& kv : oxton::ev_table) {
        kv.second.erase(comm);
    }

    return EXIT_SUCCESS;
}

int initialize()
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
