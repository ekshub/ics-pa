#include "nemu.h"
#include "monitor/monitor.h"
#include <unistd.h>
#include <sys/prctl.h>
#include <signal.h>

#include "protocol.h"
#include <stdlib.h>

bool gdb_connect_qemu(void);
bool gdb_memcpy_to_qemu(uint32_t, void *, int);
bool gdb_getregs(union gdb_regs *);
bool gdb_setregs(union gdb_regs *);
bool gdb_si(void);
void gdb_exit(void);

static bool is_skip_qemu;
static bool is_skip_nemu;

void diff_test_skip_qemu() { is_skip_qemu = true; }
void diff_test_skip_nemu() { is_skip_nemu = true; }

#define DIFF_TRACE_SIZE 16
#define DIFF_TRACE_ASM_SIZE 96

typedef struct {
  uint32_t eip;
  uint32_t next_eip;
  char assembly[DIFF_TRACE_ASM_SIZE];
} DiffTrace;

static DiffTrace diff_trace[DIFF_TRACE_SIZE];
static uint32_t diff_trace_count;

void difftest_record_trace(uint32_t eip, uint32_t next_eip, const char *assembly) {
  DiffTrace *trace = &diff_trace[diff_trace_count % DIFF_TRACE_SIZE];
  trace->eip = eip;
  trace->next_eip = next_eip;
  strncpy(trace->assembly, assembly, DIFF_TRACE_ASM_SIZE - 1);
  trace->assembly[DIFF_TRACE_ASM_SIZE - 1] = '\0';
  diff_trace_count ++;
}

#define regcpy_from_nemu(regs) \
  do { \
    regs.eax = cpu.eax; \
    regs.ecx = cpu.ecx; \
    regs.edx = cpu.edx; \
    regs.ebx = cpu.ebx; \
    regs.esp = cpu.esp; \
    regs.ebp = cpu.ebp; \
    regs.esi = cpu.esi; \
    regs.edi = cpu.edi; \
    regs.eip = cpu.eip; \
  } while (0)

static uint8_t mbr[] = {
  // start16:
  0xfa,                           // cli
  0x31, 0xc0,                     // xorw   %ax,%ax
  0x8e, 0xd8,                     // movw   %ax,%ds
  0x8e, 0xc0,                     // movw   %ax,%es
  0x8e, 0xd0,                     // movw   %ax,%ss
  0x0f, 0x01, 0x16, 0x44, 0x7c,   // lgdt   gdtdesc
  0x0f, 0x20, 0xc0,               // movl   %cr0,%eax
  0x66, 0x83, 0xc8, 0x01,         // orl    $CR0_PE,%eax
  0x0f, 0x22, 0xc0,               // movl   %eax,%cr0
  0xea, 0x1d, 0x7c, 0x08, 0x00,   // ljmp   $GDT_ENTRY(1),$start32

  // start32:
  0x66, 0xb8, 0x10, 0x00,         // movw   $0x10,%ax
  0x8e, 0xd8,                     // movw   %ax, %ds
  0x8e, 0xc0,                     // movw   %ax, %es
  0x8e, 0xd0,                     // movw   %ax, %ss
  0xeb, 0xfe,                     // jmp    7c27
  0x8d, 0x76, 0x00,               // lea    0x0(%esi),%esi

  // GDT
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x9a, 0xcf, 0x00,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xcf, 0x00,

  // GDT descriptor
  0x17, 0x00, 0x2c, 0x7c, 0x00, 0x00
};

void init_difftest(void) {
  int ppid_before_fork = getpid();
  int pid = fork();
  if (pid == -1) {
    perror("fork");
    panic("fork error");
  }
  else if (pid == 0) {
    // child

    // install a parent death signal in the chlid
    int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (r == -1) {
      perror("prctl error");
      panic("prctl");
    }

    if (getppid() != ppid_before_fork) {
      panic("parent has died!");
    }

    close(STDIN_FILENO);
    execlp("qemu-system-i386", "qemu-system-i386", "-S", "-s", "-nographic", NULL);
    perror("exec");
    panic("exec error");
  }
  else {
    // father

    gdb_connect_qemu();
    Log("Connect to QEMU successfully");

    atexit(gdb_exit);

    // put the MBR code to QEMU to enable protected mode
    bool ok = gdb_memcpy_to_qemu(0x7c00, mbr, sizeof(mbr));
    assert(ok == 1);

    union gdb_regs r;
    gdb_getregs(&r);

    // set cs:eip to 0000:7c00
    r.eip = 0x7c00;
    r.cs = 0x0000;
    ok = gdb_setregs(&r);
    assert(ok == 1);

    // execute enough instructions to enter protected mode
    int i;
    for (i = 0; i < 20; i ++) {
      gdb_si();
    }
  }
}

void init_qemu_reg() {
  union gdb_regs r;
  gdb_getregs(&r);
  regcpy_from_nemu(r);
  bool ok = gdb_setregs(&r);
  assert(ok == 1);
}

static void print_reg_diff(const char *name, uint32_t nemu, uint32_t qemu, bool *diff) {
  if (nemu != qemu) {
    printf("  %-6s nemu=0x%08x qemu=0x%08x\n", name, nemu, qemu);
    *diff = true;
  }
}

static void print_difftest_trace(void) {
  uint32_t total = diff_trace_count < DIFF_TRACE_SIZE ? diff_trace_count : DIFF_TRACE_SIZE;
  uint32_t start = diff_trace_count > total ? diff_trace_count - total : 0;

  printf("Recent instructions:\n");
  for (uint32_t i = 0; i < total; i ++) {
    DiffTrace *trace = &diff_trace[(start + i) % DIFF_TRACE_SIZE];
    printf("  0x%08x -> 0x%08x  %s\n",
        trace->eip, trace->next_eip, trace->assembly);
  }
}

#ifdef DIFF_EFLAGS
static uint32_t comparable_eflags(uint32_t eflags) {
  const uint32_t mask =
    (1u << 0) |   // CF
    (1u << 2) |   // PF
    (1u << 6) |   // ZF
    (1u << 7) |   // SF
    (1u << 9) |   // IF
    (1u << 11);   // OF
  return eflags & mask;
}
#endif

void difftest_step(uint32_t eip) {
  union gdb_regs r;
  bool diff = false;

  if (is_skip_nemu) {
    is_skip_nemu = false;
    return;
  }

  if (is_skip_qemu) {
    // to skip the checking of an instruction, just copy the reg state to qemu
    gdb_getregs(&r);
    regcpy_from_nemu(r);
    gdb_setregs(&r);
    is_skip_qemu = false;
    return;
  }

  gdb_si();
  gdb_getregs(&r);

  print_reg_diff("eax", cpu.eax, r.eax, &diff);
  print_reg_diff("ecx", cpu.ecx, r.ecx, &diff);
  print_reg_diff("edx", cpu.edx, r.edx, &diff);
  print_reg_diff("ebx", cpu.ebx, r.ebx, &diff);
  print_reg_diff("esp", cpu.esp, r.esp, &diff);
  print_reg_diff("ebp", cpu.ebp, r.ebp, &diff);
  print_reg_diff("esi", cpu.esi, r.esi, &diff);
  print_reg_diff("edi", cpu.edi, r.edi, &diff);
  print_reg_diff("eip", cpu.eip, r.eip, &diff);

#ifdef DIFF_EFLAGS
  print_reg_diff("eflags",
      comparable_eflags(cpu.eflags),
      comparable_eflags(r.eflags),
      &diff);
#endif

  if (diff) {
    printf("difftest mismatch after instruction at eip=0x%08x\n", eip);
    print_difftest_trace();
    nemu_state = NEMU_END;
  }
}
