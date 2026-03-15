#include "monitor/breakpoint.h"
#include "memory/memory.h"
#include "cpu/reg.h"
#include <stdio.h>
#include <string.h>

#define NR_BP BP_MAX_COUNT

static BP bp_pool[NR_BP];
static BP *head, *free_;

void init_bp_pool() {
  int i;
  for (i = 0; i < NR_BP; i ++) {
    bp_pool[i].NO = i;
    bp_pool[i].next = &bp_pool[i + 1];
    bp_pool[i].enabled = false;
  }
  bp_pool[NR_BP - 1].next = NULL;

  head = NULL;
  free_ = bp_pool;
}

BP* new_bp(void) {
  Assert(free_ != NULL, "No free breakpoint available");

  BP *bp = free_;
  free_ = free_->next;

  bp->next = head;
  head = bp;

  bp->addr = 0;
  bp->orig_byte = 0;
  bp->enabled = false;

  return bp;
}

void free_bp(BP *bp) {
  Assert(bp != NULL, "breakpoint is NULL");

  /* First restore original instruction if enabled */
  if (bp->enabled) {
    vaddr_write(bp->addr, 1, bp->orig_byte);
  }

  /* Remove from active list */
  BP *prev = NULL;
  BP *cur = head;
  while (cur != NULL && cur != bp) {
    prev = cur;
    cur = cur->next;
  }

  if (cur == NULL) {
    /* Breakpoint not in active list */
    return;
  }

  if (prev == NULL) {
    head = head->next;
  } else {
    prev->next = cur->next;
  }

  /* Add to free list */
  bp->next = free_;
  free_ = bp;

  bp->enabled = false;
}

BP* add_breakpoint(vaddr_t addr, bool *success) {
  /* Check if breakpoint already exists at this address */
  BP *cur = head;
  while (cur != NULL) {
    if (cur->addr == addr) {
      if (success != NULL) *success = false;
      printf("Breakpoint already exists at address 0x%08x\n", addr);
      return NULL;
    }
    cur = cur->next;
  }

  /* Read original instruction byte */
  uint8_t orig_byte = vaddr_read(addr, 1);

  /* Check if it's already an int3 instruction (0xCC) */
  if (orig_byte == 0xCC) {
    if (success != NULL) *success = false;
    printf("Address 0x%08x already contains int3 instruction\n", addr);
    return NULL;
  }

  BP *bp = new_bp();
  if (bp == NULL) {
    if (success != NULL) *success = false;
    return NULL;
  }

  bp->addr = addr;
  bp->orig_byte = orig_byte;
  bp->enabled = true;

  /* Replace with int3 instruction */
  vaddr_write(addr, 1, 0xCC);

  if (success != NULL) *success = true;
  return bp;
}

bool delete_breakpoint(int no) {
  Assert(no >= 0, "invalid breakpoint number %d", no);

  BP *cur = head;
  while (cur != NULL) {
    if (cur->NO == no) {
      free_bp(cur);
      return true;
    }
    cur = cur->next;
  }

  printf("No breakpoint #%d\n", no);
  return false;
}

void enable_breakpoint(int no) {
  Assert(no >= 0, "invalid breakpoint number %d", no);

  BP *cur = head;
  while (cur != NULL) {
    if (cur->NO == no) {
      if (!cur->enabled) {
        /* Read current byte to ensure it's not already modified */
        uint8_t current_byte = vaddr_read(cur->addr, 1);
        if (current_byte == cur->orig_byte) {
          vaddr_write(cur->addr, 1, 0xCC);
          cur->enabled = true;
          printf("Breakpoint #%d enabled\n", no);
        } else {
          printf("Cannot enable breakpoint #%d: memory changed\n", no);
        }
      } else {
        printf("Breakpoint #%d is already enabled\n", no);
      }
      return;
    }
    cur = cur->next;
  }

  printf("No breakpoint #%d\n", no);
}

void disable_breakpoint(int no) {
  Assert(no >= 0, "invalid breakpoint number %d", no);

  BP *cur = head;
  while (cur != NULL) {
    if (cur->NO == no) {
      if (cur->enabled) {
        vaddr_write(cur->addr, 1, cur->orig_byte);
        cur->enabled = false;
        printf("Breakpoint #%d disabled\n", no);
      } else {
        printf("Breakpoint #%d is already disabled\n", no);
      }
      return;
    }
    cur = cur->next;
  }

  printf("No breakpoint #%d\n", no);
}

void display_breakpoints(void) {
  if (head == NULL) {
    printf("No breakpoints\n");
    return;
  }

  printf("Num\tAddress\t\tEnabled\tOriginal\n");
  BP *cur = head;
  while (cur != NULL) {
    printf("%d\t0x%08x\t%s\t0x%02x\n",
           cur->NO, cur->addr,
           cur->enabled ? "yes" : "no",
           cur->orig_byte);
    cur = cur->next;
  }
}

BP* find_breakpoint_at(vaddr_t addr) {
  BP *cur = head;
  while (cur != NULL) {
    if (cur->addr == addr && cur->enabled) {
      return cur;
    }
    cur = cur->next;
  }
  return NULL;
}

bool check_breakpoint_hit(vaddr_t eip) {
  BP *bp = find_breakpoint_at(eip);
  if (bp != NULL) {
    /* Restore original instruction for single-step execution */
    vaddr_write(bp->addr, 1, bp->orig_byte);
    printf("Breakpoint #%d at 0x%08x\n", bp->NO, bp->addr);
    return true;
  }
  return false;
}

void reenable_breakpoints(void) {
  BP *cur = head;
  while (cur != NULL) {
    if (cur->enabled) {
      /* Check current byte at breakpoint address */
      uint8_t current_byte = vaddr_read(cur->addr, 1);
      printf("[reenable] breakpoint #%d at 0x%08x: orig=0x%02x current=0x%02x\n",
             cur->NO, cur->addr, cur->orig_byte, current_byte);

      /* Check if program is currently stopped at this breakpoint */
      if (cpu.eip == cur->addr) {
        /* Program is stopped at breakpoint address, do not re-enable int3 yet */
        printf("[reenable] program stopped at breakpoint #%d (0x%08x), skipping int3\n",
               cur->NO, cur->addr);
      } else if (current_byte == cur->orig_byte) {
        /* Original instruction, need to set int3 */
        vaddr_write(cur->addr, 1, 0xCC);
      } else if (current_byte == 0xCC) {
        /* Already int3, breakpoint is active */
        /* Do nothing */
      } else {
        /* Memory changed to something else, disable breakpoint */
        printf("Warning: Memory at breakpoint #%d (0x%08x) changed from 0x%02x to 0x%02x, disabling\n",
               cur->NO, cur->addr, cur->orig_byte, current_byte);
        cur->enabled = false;
      }
    }
    cur = cur->next;
  }
}