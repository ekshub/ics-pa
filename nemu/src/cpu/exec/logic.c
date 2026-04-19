#include "cpu/exec.h"

make_EHelper(test) {
  rtl_and(&t2, &id_dest->val, &id_src->val);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);

  rtl_set_CF(&tzero);
  rtl_set_OF(&tzero);

  print_asm_template2(test);
}

make_EHelper(and) {
  rtl_and(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);

  rtl_set_CF(&tzero);
  rtl_set_OF(&tzero);

  print_asm_template2(and);
}

make_EHelper(xor) {
  rtl_xor(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);

  // CF and OF are cleared by XOR
  rtl_set_CF(&tzero);
  rtl_set_OF(&tzero);

  print_asm_template2(xor);
}

make_EHelper(or) {
  rtl_or(&t2, &id_dest->val, &id_src->val);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);

  rtl_set_CF(&tzero);
  rtl_set_OF(&tzero);

  print_asm_template2(or);
}

make_EHelper(rol) {
  uint8_t shamt = id_src->val & 0x1f;
  if (shamt == 0) {
    print_asm_template2(rol);
    return;
  }

  uint32_t width_bits = id_dest->width * 8;
  uint32_t mask = (id_dest->width == 4) ? 0xffffffffu : ((1u << width_bits) - 1);
  uint32_t value = id_dest->val & mask;
  shamt %= width_bits;
  value = ((value << shamt) | (value >> (width_bits - shamt))) & mask;
  rtl_li(&t2, value);
  operand_write(id_dest, &t2);

  print_asm_template2(rol);
}

make_EHelper(ror) {
  uint8_t shamt = id_src->val & 0x1f;
  if (shamt == 0) {
    print_asm_template2(ror);
    return;
  }

  uint32_t width_bits = id_dest->width * 8;
  uint32_t mask = (id_dest->width == 4) ? 0xffffffffu : ((1u << width_bits) - 1);
  uint32_t value = id_dest->val & mask;
  shamt %= width_bits;
  value = ((value >> shamt) | (value << (width_bits - shamt))) & mask;
  rtl_li(&t2, value);
  operand_write(id_dest, &t2);

  print_asm_template2(ror);
}

make_EHelper(sar) {
  uint8_t shamt = id_src->val & 0x1f;
  if (shamt == 0) {
    print_asm_template2(sar);
    return;
  }

  rtl_li(&t0, shamt);
  rtl_sar(&t2, &id_dest->val, &t0);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);
  // unnecessary to update CF and OF in NEMU

  print_asm_template2(sar);
}

make_EHelper(shl) {
  uint8_t shamt = id_src->val & 0x1f;
  if (shamt == 0) {
    print_asm_template2(shl);
    return;
  }

  rtl_li(&t0, shamt);
  rtl_shl(&t2, &id_dest->val, &t0);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);
  // unnecessary to update CF and OF in NEMU

  print_asm_template2(shl);
}

make_EHelper(shr) {
  uint8_t shamt = id_src->val & 0x1f;
  if (shamt == 0) {
    print_asm_template2(shr);
    return;
  }

  rtl_li(&t0, shamt);
  rtl_shr(&t2, &id_dest->val, &t0);
  operand_write(id_dest, &t2);

  rtl_update_ZFSF(&t2, id_dest->width);
  rtl_update_PF(&t2);
  // unnecessary to update CF and OF in NEMU

  print_asm_template2(shr);
}

make_EHelper(setcc) {
  uint8_t subcode = decoding.opcode & 0xf;
  rtl_setcc(&t2, subcode);
  operand_write(id_dest, &t2);

  print_asm("set%s %s", get_cc_name(subcode), id_dest->str);
}

make_EHelper(not) {
  rtl_not(&id_dest->val);
  operand_write(id_dest, &id_dest->val);

  print_asm_template1(not);
}
