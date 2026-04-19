#include "nemu.h"
#include "monitor/monitor.h"
#include "monitor/watchpoint.h"
#include "monitor/breakpoint.h"

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INSTR_TO_PRINT 10
#define DEADLOOP_MAX_PERIOD 8
#define DEADLOOP_REPEAT_THRESHOLD 100000

typedef struct {
  vaddr_t eip;
  uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
} CPU_snapshot;

static CPU_snapshot deadloop_history[DEADLOOP_MAX_PERIOD];
static uint32_t deadloop_hist_count = 0;
static uint32_t deadloop_hist_head = 0;
static uint32_t deadloop_matched_period = 0;
static uint32_t deadloop_repeat_count = 0;

static inline void take_snapshot(CPU_snapshot *s) {
  s->eip = cpu.eip;
  s->eax = cpu.eax;
  s->ecx = cpu.ecx;
  s->edx = cpu.edx;
  s->ebx = cpu.ebx;
  s->esp = cpu.esp;
  s->ebp = cpu.ebp;
  s->esi = cpu.esi;
  s->edi = cpu.edi;
}

static inline bool same_snapshot(const CPU_snapshot *a, const CPU_snapshot *b) {
  return a->eip == b->eip &&
         a->eax == b->eax &&
         a->ecx == b->ecx &&
         a->edx == b->edx &&
         a->ebx == b->ebx &&
         a->esp == b->esp &&
         a->ebp == b->ebp &&
         a->esi == b->esi &&
         a->edi == b->edi;
}

static void reset_deadloop_detector(void) {
  deadloop_hist_count = 0;
  deadloop_hist_head = 0;
  deadloop_matched_period = 0;
  deadloop_repeat_count = 0;
}

static bool detect_deadloop(void) {
  CPU_snapshot cur;

  take_snapshot(&cur);

  uint32_t found_period = 0;
  uint32_t max_period = deadloop_hist_count < DEADLOOP_MAX_PERIOD ? deadloop_hist_count : DEADLOOP_MAX_PERIOD;
  for (uint32_t period = 1; period <= max_period; period++) {
    uint32_t idx = (deadloop_hist_head + DEADLOOP_MAX_PERIOD - period) % DEADLOOP_MAX_PERIOD;
    if (same_snapshot(&cur, &deadloop_history[idx])) {
      found_period = period;
      break;
    }
  }

  deadloop_history[deadloop_hist_head] = cur;
  deadloop_hist_head = (deadloop_hist_head + 1) % DEADLOOP_MAX_PERIOD;
  if (deadloop_hist_count < DEADLOOP_MAX_PERIOD) {
    deadloop_hist_count++;
  }

  if (found_period == 0) {
    deadloop_matched_period = 0;
    deadloop_repeat_count = 0;
    return false;
  }

  if (deadloop_matched_period == found_period) {
    deadloop_repeat_count++;
  } else {
    deadloop_matched_period = found_period;
    deadloop_repeat_count = 1;
  }

  return (uint64_t)deadloop_matched_period * deadloop_repeat_count >= DEADLOOP_REPEAT_THRESHOLD;
}

int nemu_state = NEMU_STOP;

void exec_wrapper(bool);

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  if (nemu_state == NEMU_END) {
    printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
    return;
  }

  /* Re-enable breakpoints before execution */
  reenable_breakpoints();

  nemu_state = NEMU_RUNNING;

  bool print_flag = n < MAX_INSTR_TO_PRINT;
  bool check_deadloop = (n == (uint64_t)-1) &&
                        !nemu_is_batch_mode() &&
                        nemu_deadloop_detection_enabled();
  reset_deadloop_detector();

  for (; n > 0; n --) {
    /* Execute one instruction, including instruction fetch,
     * instruction decode, and the actual execution. */
    exec_wrapper(print_flag);

#ifdef DEBUG
    if (check_watchpoints()) {
      nemu_state = NEMU_STOP;
    }

#endif

#ifdef HAS_IOE
    extern void device_update();
    device_update();
#endif

    if (check_deadloop && detect_deadloop()) {
      nemu_state = NEMU_STOP;
      printf("Possible dead loop detected near eip = 0x%08x.\n", cpu.eip);
      printf("Execution has been paused. Use `si` to inspect the loop or `c` to continue.\n");
    }

    if (nemu_state != NEMU_RUNNING) { return; }
  }

  if (nemu_state == NEMU_RUNNING) { nemu_state = NEMU_STOP; }
}
