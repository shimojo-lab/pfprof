#ifndef __OXTON_HPP__
#define __OXTON_HPP__

#include <unordered_set>

extern "C" {
#include <mpi.h>
#include <peruse.h>
};


#include "trace.hpp"

#define NUM_REQ_EVENT_NAMES (2)


namespace oxton {

// PERUSE event descriptor
typedef int event_desc_t;
// Mapping from communicators to PERUSE event handlers
typedef std::unordered_map<MPI_Comm, peruse_event_h> eh_table_t;
// Mapping from PERUSE event descriptors to event handler tables
typedef std::unordered_map<event_desc_t, eh_table_t> ev_table_t;

// List of communicators
extern std::unordered_set<MPI_Comm> comms;
// Mapping from communicator to local-global rank table
extern std::unordered_map<MPI_Comm, std::vector<int>> lg_rank_table;
extern ev_table_t ev_table;

extern trace trace;

int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
                         peruse_comm_spec_t *spec, void *param);
int register_event_handlers(MPI_Comm comm, peruse_comm_callback_f *callback);
int remove_event_handlers(MPI_Comm comm);
int register_comm(MPI_Comm comm);
int unregister_comm(MPI_Comm comm);
int initialize();

}

#endif
