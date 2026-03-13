#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expr[128];
  uint32_t last_val;


} WP;

void init_wp_pool(void);
WP* new_wp(void);
void free_wp(WP *wp);
WP* watchpoint_head(void);
WP* add_watchpoint(const char *expr, bool *success);
bool delete_watchpoint(int no);
void display_watchpoints(void);
bool check_watchpoints(void);

#endif
