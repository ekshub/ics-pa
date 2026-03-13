#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <stdio.h>

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

void display_watchpoints(void) {
  WP *cur = head;
  if (cur == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("Num\tValue\t\tExpr\n");
  while (cur != NULL) {
    printf("%d\t0x%08x\t%s\n", cur->NO, cur->last_val, cur->expr);
    cur = cur->next;
  }
}

bool check_watchpoints(void) {
  bool triggered = false;
  WP *cur = head;
  while (cur != NULL) {
    bool success = true;
    uint32_t new_val = expr(cur->expr, &success);
    if (!success) {
      cur = cur->next;
      continue;
    }

    if (new_val != cur->last_val) {
      printf("Watchpoint %d triggered: %s\n", cur->NO, cur->expr);
      printf("Old value = 0x%08x\n", cur->last_val);
      printf("New value = 0x%08x\n", new_val);
      triggered = true;
    }

    cur = cur->next;
  }

  return triggered;
}


