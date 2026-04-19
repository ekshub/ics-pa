#include "cpu/exec.h"

make_EHelper(mov);
make_EHelper(lea);
make_EHelper(movsx);
make_EHelper(movzx);
make_EHelper(push);
make_EHelper(pop);
make_EHelper(leave);
make_EHelper(cltd);
make_EHelper(cwtl);
make_EHelper(call);
make_EHelper(call_rm);
make_EHelper(jmp);
make_EHelper(jcc);
make_EHelper(jmp_rm);
make_EHelper(sub);
make_EHelper(xor);
make_EHelper(ret);

// arithmetic
make_EHelper(add);
make_EHelper(adc);
make_EHelper(sbb);
make_EHelper(cmp);

// logic
make_EHelper(and);
make_EHelper(or);
make_EHelper(shl);
make_EHelper(shr);
make_EHelper(sar);
make_EHelper(setcc);

make_EHelper(operand_size);
make_EHelper(nop);

make_EHelper(inv);
make_EHelper(nemu_trap);
make_EHelper(int3);
