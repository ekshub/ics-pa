#include "trap.h"

int main() {
  asm volatile (
    "xor %%eax, %%eax\n"
    "1:\n"
    "incl %%eax\n"
    "decl %%eax\n"
    "nop\n"
    "jmp 1b\n"
    :
    :
    : "eax"
  );

  return 0;
}
