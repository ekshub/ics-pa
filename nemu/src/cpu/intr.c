#include "cpu/exec.h"
#include "memory/mmu.h"

extern uint32_t idtr_base;
extern uint16_t idtr_limit;

void raise_intr(uint8_t NO, vaddr_t ret_addr) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * That is, use ``NO'' to index the IDT.
   */
  uint32_t gate_addr = idtr_base + NO * 8;
  Assert(NO * 8 + 7 <= idtr_limit, "IDT out of bound: NO=%d limit=%x", NO, idtr_limit);

  uint32_t low = vaddr_read(gate_addr, 4);
  uint32_t high = vaddr_read(gate_addr + 4, 4);
  uint32_t target = (low & 0xffffu) | (high & 0xffff0000u);

  rtl_push(&cpu.eflags);
  rtl_push(&tzero);
  rtl_push(&ret_addr);

  decoding.jmp_eip = target;
  decoding.is_jmp = 1;
}

void dev_raise_intr() {
  const uint8_t irq_timer = 32;
  if (!cpu.IF) {
    return;
  }
  raise_intr(irq_timer, cpu.eip);
}
