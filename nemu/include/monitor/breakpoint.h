#ifndef __BREAKPOINT_H__
#define __BREAKPOINT_H__

#include "common.h"

#define BP_MAX_COUNT 32

typedef struct breakpoint {
  int NO;
  struct breakpoint *next;

  /* Address of breakpoint */
  vaddr_t addr;

  /* Original instruction byte (first byte) */
  uint8_t orig_byte;

  /* Whether breakpoint is enabled */
  bool enabled;

} BP;

void init_bp_pool(void);
BP* new_bp(void);
void free_bp(BP *bp);
BP* add_breakpoint(vaddr_t addr, bool *success);
bool delete_breakpoint(int no);
void enable_breakpoint(int no);
void disable_breakpoint(int no);
void display_breakpoints(void);
BP* find_breakpoint_at(vaddr_t addr);
bool check_breakpoint_hit(vaddr_t eip);
void reenable_breakpoints(void);

#endif