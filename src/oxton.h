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

void write_xfer_event(xfer_event_t *ev);
void init_writer();
void close_writer();

extern int num_procs, my_rank;
