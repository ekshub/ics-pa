#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <stdio.h>
#include <string.h>

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
  wp->condition[0] = '\0';
  wp->has_condition = false;

  return wp;
}

void free_wp(WP *wp) {
  Assert(wp != NULL, "watchpoint is NULL");
  
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
    Assert(cur->expr[0] != '\0', "watchpoint %d has empty expression", cur->NO);
    printf("%d\t0x%08x\t%s", cur->NO, cur->last_val, cur->expr);
    if (cur->has_condition) {
      printf(" if %s", cur->condition);
    }
    printf("\n");
    cur = cur->next;
  }
}

bool check_watchpoints(void) {
  bool triggered = false;
  WP *cur = head;
  while (cur != NULL) {
    Assert(cur->expr[0] != '\0', "watchpoint %d has empty expression", cur->NO);
    bool success = true;
    uint32_t new_val = expr(cur->expr, &success);
    if (!success) {
      cur = cur->next;
      continue;
    }

    if (new_val != cur->last_val) {
      /* Check condition if this is a conditional watchpoint */
      bool should_trigger = true;
      if (cur->has_condition) {
        bool cond_success = true;
        uint32_t cond_val = expr(cur->condition, &cond_success);
        if (!cond_success) {
          /* Condition evaluation failed, skip this watchpoint */
          cur->last_val = new_val;
          cur = cur->next;
          continue;
        }
        /* Condition is true if value is non-zero */
        should_trigger = (cond_val != 0);
      }

      if (should_trigger) {
        printf("Watchpoint %d triggered: %s", cur->NO, cur->expr);
        if (cur->has_condition) {
          printf(" if %s", cur->condition);
        }
        printf("\n");
        printf("Old value = 0x%08x\n", cur->last_val);
        printf("New value = 0x%08x\n", new_val);
        cur->last_val = new_val;
        triggered = true;
      } else {
        /* Update value but don't trigger */
        cur->last_val = new_val;
      }
    }

    cur = cur->next;
  }

  return triggered;
}

WP* add_watchpoint(const char *expr_str, const char *condition_str, bool *success) {
  Assert(expr_str != NULL, "watchpoint expression is NULL");
  Assert(strlen(expr_str) < sizeof(((WP *)0)->expr), "watchpoint expression too long");

  WP *wp = new_wp();
  strncpy(wp->expr, expr_str, sizeof(wp->expr) - 1);
  wp->expr[sizeof(wp->expr) - 1] = '\0';

  /* Initialize condition */
  wp->has_condition = false;
  wp->condition[0] = '\0';

  /* Set condition if provided */
  if (condition_str != NULL && condition_str[0] != '\0') {
    Assert(strlen(condition_str) < sizeof(((WP *)0)->condition), "watchpoint condition too long");
    strncpy(wp->condition, condition_str, sizeof(wp->condition) - 1);
    wp->condition[sizeof(wp->condition) - 1] = '\0';
    wp->has_condition = true;

    /* Validate condition expression */
    bool cond_ok = true;
    expr(wp->condition, &cond_ok);
    if (!cond_ok) {
      free_wp(wp);
      if (success != NULL) {
        *success = false;
      }
      return NULL;
    }
  }

  /* Validate main expression */
  bool ok = true;
  wp->last_val = expr(wp->expr, &ok);
  if (!ok) {
    free_wp(wp);
    if (success != NULL) {
      *success = false;
    }
    return NULL;
  }

  if (success != NULL) {
    *success = true;
  }
  return wp;
}

bool delete_watchpoint(int no) {
  Assert(no >= 0, "invalid watchpoint number %d", no);
  WP *cur = head;
  while (cur != NULL) {
    if (cur->NO == no) {
      free_wp(cur);
      return true;
    }
    cur = cur->next;
  }

  return false;
}


