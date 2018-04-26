#ifndef __OXTON_HPP__
#define __OXTON_HPP__

#include <unordered_set>

extern "C" {
#include <mpi.h>
#include <peruse.h>
};


#include "trace.hpp"

#define NUM_REQ_EVENT_NAMES (2)


namespace pfprof {

int peruse_event_handler(peruse_event_h event_handle, MPI_Aint unique_id,
                         peruse_comm_spec_t *spec, void *param);
int register_event_handlers(MPI_Comm comm, peruse_comm_callback_f *callback);
int remove_event_handlers(MPI_Comm comm);
int register_comm(MPI_Comm comm);
int unregister_comm(MPI_Comm comm);
int initialize();
int finalize();

}

#endif
