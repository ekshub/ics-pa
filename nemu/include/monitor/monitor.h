#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "common.h"

enum { NEMU_STOP, NEMU_RUNNING, NEMU_END };
extern int nemu_state;

bool nemu_is_batch_mode(void);
void nemu_enable_deadloop_detection(bool enable);
bool nemu_deadloop_detection_enabled(void);

#endif
