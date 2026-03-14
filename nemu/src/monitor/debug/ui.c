#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "monitor/breakpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint64_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char *args) {
  uint64_t n = 1;
  if (args != NULL) {
    char *endptr = NULL;
    unsigned long value = strtoul(args, &endptr, 10);
    if (endptr == args || *endptr != '\0') {
      printf("Usage: si [N]\n");
      return 0;
    }
    n = value;
  }
  cpu_exec(n);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf("Usage: info r|w|b\n");
    return 0;
  }

  char *subcmd = strtok(args, " ");
  if (subcmd == NULL) {
    printf("Usage: info r|w|b\n");
    return 0;
  }

  if (strcmp(subcmd, "r") == 0) {
    int i;
    for (i = 0; i < 8; i ++) {
      printf("%3s\t0x%08x\n", regsl[i], reg_l(i));
    }
    printf("eip\t0x%08x\n", cpu.eip);
    return 0;
  }

  if (strcmp(subcmd, "w") == 0) {
    display_watchpoints();
    return 0;
  }

  if (strcmp(subcmd, "b") == 0) {
    display_breakpoints();
    return 0;
  }

  printf("Unknown info subcommand '%s'\n", subcmd);
  return 0;
}

static int cmd_x(char *args) {
  if (args == NULL) {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  char *n_str = strtok(args, " ");
  char *expr_str = strtok(NULL, " ");
  if (n_str == NULL || expr_str == NULL) {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  char *endptr_n = NULL;
  unsigned long n = strtoul(n_str, &endptr_n, 10);
  if (endptr_n == n_str || *endptr_n != '\0') {
    printf("Invalid N: %s\n", n_str);
    return 0;
  }

  char *endptr_expr = NULL;
  vaddr_t addr = (vaddr_t)strtoul(expr_str, &endptr_expr, 0);
  if (endptr_expr == expr_str || *endptr_expr != '\0') {
    printf("Invalid EXPR: %s\n", expr_str);
    return 0;
  }

  uint32_t i;
  for (i = 0; i < n; i++) {
    vaddr_t cur = addr + i * 4;
    uint32_t data = vaddr_read(cur, 4);
    printf("0x%08x: 0x%08x\n", cur, data);
  }

  return 0;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n");
    return 0;
  }

  while (*args == ' ') {
    args ++;
  }
  if (*args == '\0') {
    printf("Usage: p EXPR\n");
    return 0;
  }

  bool success = true;
  uint32_t value = expr(args, &success);
  if (!success) {
    printf("Bad expression: %s\n", args);
    return 0;
  }

  printf("0x%08x (%u)\n", value, value);
  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL) {
    printf("Usage: w EXPR [if CONDITION]\n");
    return 0;
  }

  while (*args == ' ') {
    args ++;
  }
  if (*args == '\0') {
    printf("Usage: w EXPR [if CONDITION]\n");
    return 0;
  }

  /* Parse expression and optional condition */
  char *expr_str = args;
  char *condition_str = NULL;

  /* Look for " if " in the arguments */
  char *if_pos = strstr(args, " if ");
  if (if_pos != NULL) {
    /* Split the string at " if " */
    *if_pos = '\0';
    condition_str = if_pos + 4; /* Skip " if " */

    /* Trim leading spaces from condition */
    while (*condition_str == ' ') {
      condition_str++;
    }

    /* Check if condition is empty */
    if (*condition_str == '\0') {
      condition_str = NULL;
    }
  }

  bool success = true;
  WP *wp = add_watchpoint(expr_str, condition_str, &success);
  if (!success || wp == NULL) {
    printf("Bad expression: %s", expr_str);
    if (condition_str != NULL) {
      printf(" if %s", condition_str);
    }
    printf("\n");
    return 0;
  }

  printf("Watchpoint %d: %s", wp->NO, wp->expr);
  if (wp->has_condition) {
    printf(" if %s", wp->condition);
  }
  printf("\n");
  return 0;
}

static int cmd_d(char *args) {
  if (args == NULL) {
    printf("Usage: d N\n");
    return 0;
  }

  char *endptr = NULL;
  long no = strtol(args, &endptr, 10);
  while (endptr != NULL && *endptr == ' ') {
    endptr ++;
  }
  if (endptr == args || *endptr != '\0' || no < 0) {
    printf("Invalid watchpoint number: %s\n", args);
    return 0;
  }

  if (!delete_watchpoint((int)no)) {
    printf("No watchpoint number %ld\n", no);
    return 0;
  }

  printf("Watchpoint %ld deleted\n", no);
  return 0;
}

/* Breakpoint commands */
static int cmd_b(char *args) {
  if (args == NULL) {
    printf("Usage: b ADDR\n");
    return 0;
  }

  bool success = true;
  uint32_t addr = expr(args, &success);
  if (!success) {
    printf("Invalid address: %s\n", args);
    return 0;
  }

  BP *bp = add_breakpoint(addr, &success);
  if (!success || bp == NULL) {
    printf("Failed to set breakpoint at 0x%08x\n", addr);
    return 0;
  }

  printf("Breakpoint %d at 0x%08x\n", bp->NO, bp->addr);
  return 0;
}

static int cmd_bl(char *args) {
  display_breakpoints();
  return 0;
}

static int cmd_bd(char *args) {
  if (args == NULL) {
    printf("Usage: bd N\n");
    return 0;
  }

  char *endptr = NULL;
  long no = strtol(args, &endptr, 10);
  while (endptr != NULL && *endptr == ' ') {
    endptr ++;
  }
  if (endptr == args || *endptr != '\0' || no < 0) {
    printf("Invalid breakpoint number: %s\n", args);
    return 0;
  }

  if (!delete_breakpoint((int)no)) {
    printf("No breakpoint number %ld\n", no);
    return 0;
  }

  printf("Breakpoint %ld deleted\n", no);
  return 0;
}

static int cmd_help(char *args);

static struct {
  char *name;
  char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "si", "Step through N instructions (default 1)", cmd_si },
  { "info", "Print program status, e.g. info r", cmd_info },
  { "x", "Scan memory: x N EXPR", cmd_x },
  { "p", "Evaluate expression: p EXPR", cmd_p },
  { "w", "Set watchpoint: w EXPR", cmd_w },
  { "d", "Delete watchpoint: d N", cmd_d },
  { "b", "Set breakpoint: b ADDR", cmd_b },
  { "bl", "List all breakpoints", cmd_bl },
  { "bd", "Delete breakpoint: bd N", cmd_bd },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void ui_mainloop(int is_batch_mode) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  while (1) {
    char *str = rl_gets();
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef HAS_IOE
    extern void sdl_clear_event_queue(void);
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}
