#ifndef __OTF2_WRITER_H__
#define __OTF2_WRITER_H__

#include <mpi.h>
#include <otf2/otf2.h>

#include "oxton.h"

extern OTF2_Archive *archive;
extern uint64_t epoch_start;
extern uint64_t epoch_end;
extern uint64_t global_epoch_start;
extern uint64_t global_epoch_end;

int open_otf2_writer();
int close_otf2_writer();

int write_begin_send_event(uint32_t dst, uint32_t tag, uint32_t len,
                            uint64_t req_id);
int write_end_send_event(uint64_t req_id);

int write_begin_recv_event(uint64_t req_id);
int write_end_recv_event(uint32_t src, uint32_t tag, uint32_t len,
                         uint64_t req_id);

int write_global_defs();

#endif
