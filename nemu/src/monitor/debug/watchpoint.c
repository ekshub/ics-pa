#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = &wp_pool[i + 1];
  }
  wp_pool[NR_WP - 1].next = NULL;

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* watchpoint_head(void) {
  return head;
}

WP* new_wp(void) {
  Assert(free_ != NULL, "No free watchpoint available");

  WP *wp = free_;
  free_ = free_->next;

  wp->next = head;
  head = wp;

  wp->expr[0] = '\0';
  wp->last_val = 0;

  return wp;
}

void free_wp(WP *wp) {
  WP *prev = NULL;
  WP *cur = head;
  while (cur != NULL && cur != wp) {
    prev = cur;
    cur = cur->next;
  }

  Assert(cur == wp, "watchpoint %d is not in active list", wp->NO);

  if (prev == NULL) {
    head = cur->next;
  }
  else {
    prev->next = cur->next;
  }

  cur->next = free_;
  free_ = cur;
}


