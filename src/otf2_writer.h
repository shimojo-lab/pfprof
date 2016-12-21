#ifndef __OTF2_WRITER_H__
#define __OTF2_WRITER_H__

#include <otf2/otf2.h>

#include "oxton.h"

extern OTF2_Archive *archive;
extern OTF2_EvtWriter* evt_writer;
extern uint64_t epoch_start;
extern uint64_t epoch_end;
extern uint64_t global_epoch_start;
extern uint64_t global_epoch_end;

int open_otf2_writer();
int close_otf2_writer();
int write_xfer_event(xfer_event_t *ev);

int write_global_defs();

#endif
