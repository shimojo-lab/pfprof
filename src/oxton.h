#ifndef __OXTON_H__
#define __OXTON_H__

#include <mpi.h>

#include "uthash.h"

typedef struct {
    MPI_Aint unique_id;
    int src;
    int dst;
    double start_time;
    double end_time;
    int size;
    UT_hash_handle hh;
} xfer_event_t;

extern int num_procs, my_rank;

#endif
