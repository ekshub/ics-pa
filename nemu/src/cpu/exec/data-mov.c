#include "cpu/exec.h"

make_EHelper(mov) {
  operand_write(id_dest, &id_src->val);
  print_asm_template2(mov);
}

make_EHelper(push) {
  rtl_push(&id_dest->val);
  print_asm_template1(push);
}

make_EHelper(pop) {
  rtl_pop(&id_dest->val);
  operand_write(id_dest, &id_dest->val);
  print_asm_template1(pop);
}

make_EHelper(pusha) {
  rtl_mv(&t0, &cpu.esp);

  rtl_lr_l(&t1, R_EAX); rtl_push(&t1);
  rtl_lr_l(&t1, R_ECX); rtl_push(&t1);
  rtl_lr_l(&t1, R_EDX); rtl_push(&t1);
  rtl_lr_l(&t1, R_EBX); rtl_push(&t1);
  rtl_push(&t0);
  rtl_lr_l(&t1, R_EBP); rtl_push(&t1);
  rtl_lr_l(&t1, R_ESI); rtl_push(&t1);
  rtl_lr_l(&t1, R_EDI); rtl_push(&t1);

  print_asm("pusha");
}

make_EHelper(popa) {
  rtl_pop(&t1); rtl_sr_l(R_EDI, &t1);
  rtl_pop(&t1); rtl_sr_l(R_ESI, &t1);
  rtl_pop(&t1); rtl_sr_l(R_EBP, &t1);
  rtl_pop(&t1);
  rtl_pop(&t1); rtl_sr_l(R_EBX, &t1);
  rtl_pop(&t1); rtl_sr_l(R_EDX, &t1);
  rtl_pop(&t1); rtl_sr_l(R_ECX, &t1);
  rtl_pop(&t1); rtl_sr_l(R_EAX, &t1);

  print_asm("popa");
}

make_EHelper(leave) {
  rtl_mv(&cpu.esp, &cpu.ebp);
  rtl_pop(&t0);
  rtl_sr_l(R_EBP, &t0);

  print_asm("leave");
}

make_EHelper(cltd) {
  if (decoding.is_operand_size_16) {
    rtl_lr_w(&t0, R_AX);
    rtl_msb(&t1, &t0, 2);
    rtl_sub(&t2, &tzero, &t1);
    rtl_sr_w(R_DX, &t2);
  }
  else {
    rtl_lr_l(&t0, R_EAX);
    rtl_msb(&t1, &t0, 4);
    rtl_sub(&t2, &tzero, &t1);
    rtl_sr_l(R_EDX, &t2);
  }

  print_asm(decoding.is_operand_size_16 ? "cwd" : "cltd");
}

make_EHelper(cwtl) {
  if (decoding.is_operand_size_16) {
    rtl_lr_b(&t0, R_AL);
    rtl_sext(&t1, &t0, 1);
    rtl_sr_w(R_AX, &t1);
  }
  else {
    rtl_lr_w(&t0, R_AX);
    rtl_sext(&t1, &t0, 2);
    rtl_sr_l(R_EAX, &t1);
  }

  print_asm(decoding.is_operand_size_16 ? "cbtw" : "cwtl");
}

make_EHelper(movsx) {
  id_dest->width = decoding.is_operand_size_16 ? 2 : 4;
  rtl_sext(&t2, &id_src->val, id_src->width);
  operand_write(id_dest, &t2);
  print_asm_template2(movsx);
}

make_EHelper(movzx) {
  id_dest->width = decoding.is_operand_size_16 ? 2 : 4;
  operand_write(id_dest, &id_src->val);
  print_asm_template2(movzx);
}

make_EHelper(lea) {
  rtl_li(&t2, id_src->addr);
  operand_write(id_dest, &t2);
  print_asm_template2(lea);
}
