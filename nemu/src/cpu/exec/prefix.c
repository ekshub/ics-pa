#include "cpu/exec.h"

make_EHelper(real);

make_EHelper(operand_size) {
  bool old = decoding.is_operand_size_16;
  decoding.is_operand_size_16 = true;
  exec_real(eip);
  decoding.is_operand_size_16 = old;
}
