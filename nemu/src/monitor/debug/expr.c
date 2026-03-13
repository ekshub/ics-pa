#include "nemu.h"
#include <stdlib.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_DEC,
  TK_HEX,
  TK_REG,
  TK_EQ,
  TK_NEQ,
  TK_AND,
  TK_NEG,
  TK_DEREF

  /* TODO: Add more token types */

};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},           // spaces
  {"==", TK_EQ},               // equal
  {"!=", TK_NEQ},              // not equal
  {"&&", TK_AND},              // and
  {"\\+", '+'},                // plus
  {"-", '-'},                  // minus
  {"\\*", '*'},                // multiply
  {"/", '/'},                  // divide
  {"\\(", '('},                // left parenthesis
  {"\\)", ')'},                // right parenthesis
  {"0[xX][0-9a-fA-F]+", TK_HEX},// hexadecimal integer
  {"[0-9]+", TK_DEC},           // decimal integer
  {"\\$[a-zA-Z][a-zA-Z0-9]*", TK_REG} // register
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;

static uint32_t reg_value(const char *name, bool *success) {
  if (strcmp(name, "eip") == 0) {
    return cpu.eip;
  }

  int i;
  for (i = 0; i < 8; i ++) {
    if (strcmp(name, regsl[i]) == 0) {
      return reg_l(i);
    }
    if (strcmp(name, regsw[i]) == 0) {
      return reg_w(i);
    }
    if (strcmp(name, regsb[i]) == 0) {
      return reg_b(i);
    }
  }

  *success = false;
  return 0;
}

static uint32_t eval_single(int p, int q, bool *success) {
  if (p != q) {
    *success = false;
    return 0;
  }

  if (tokens[p].type == TK_DEC) {
    return strtoul(tokens[p].str, NULL, 10);
  }
  if (tokens[p].type == TK_HEX) {
    return strtoul(tokens[p].str, NULL, 16);
  }
  if (tokens[p].type == TK_REG) {
    return reg_value(tokens[p].str + 1, success);
  }

  *success = false;
  return 0;
}

static bool check_parentheses(int p, int q) {
  if (tokens[p].type != '(' || tokens[q].type != ')') {
    return false;
  }

  int level = 0;
  int i;
  for (i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      level ++;
    }
    else if (tokens[i].type == ')') {
      level --;
      if (level < 0) {
        return false;
      }
      if (level == 0 && i < q) {
        return false;
      }
    }
  }

  return level == 0;
}

static int precedence(int type) {
  switch (type) {
    case TK_AND: return 1;
    case TK_EQ:
    case TK_NEQ: return 2;
    case '+':
    case '-': return 3;
    case '*':
    case '/': return 4;
    case TK_NEG:
    case TK_DEREF: return 5;
    default: return 100;
  }
}

static bool is_unary(int type) {
  return type == TK_NEG || type == TK_DEREF;
}

static uint32_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }

  if (p == q) {
    return eval_single(p, q, success);
  }

  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }

  int level = 0;
  int op = -1;
  int min_prec = 101;
  int i;
  for (i = p; i <= q; i ++) {
    int type = tokens[i].type;
    if (type == '(') {
      level ++;
      continue;
    }
    if (type == ')') {
      level --;
      if (level < 0) {
        *success = false;
        return 0;
      }
      continue;
    }
    if (level != 0) {
      continue;
    }

    int prec = precedence(type);
    if (prec < min_prec) {
      min_prec = prec;
      op = i;
      continue;
    }

    if (prec == min_prec) {
      if (!is_unary(type)) {
        op = i;
      }
    }
  }

  if (level != 0 || op == -1) {
    *success = false;
    return 0;
  }

  if (tokens[op].type == TK_NEG) {
    uint32_t val = eval(op + 1, q, success);
    if (!*success) {
      return 0;
    }
    return -val;
  }

  if (tokens[op].type == TK_DEREF) {
    uint32_t addr = eval(op + 1, q, success);
    if (!*success) {
      return 0;
    }
    return vaddr_read(addr, 4);
  }

  uint32_t val1 = eval(p, op - 1, success);
  if (!*success) {
    return 0;
  }
  uint32_t val2 = eval(op + 1, q, success);
  if (!*success) {
    return 0;
  }

  switch (tokens[op].type) {
    case TK_AND: return val1 && val2;
    case TK_EQ: return val1 == val2;
    case TK_NEQ: return val1 != val2;
    case '+': return val1 + val2;
    case '-': return val1 - val2;
    case '*': return val1 * val2;
    case '/':
      if (val2 == 0) {
        *success = false;
        return 0;
      }
      return val1 / val2;
    default:
      *success = false;
      return 0;
  }

  *success = false;
  return 0;
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;
          case TK_DEC:
          case TK_HEX:
          case TK_REG:
          case TK_EQ:
          case TK_NEQ:
          case TK_AND:
          case '+':
          case '-':
          case '*':
          case '/':
          case '(':
          case ')': {
            Assert(nr_token < (int)(sizeof(tokens) / sizeof(tokens[0])),
                "too many tokens in expression");
            tokens[nr_token].type = rules[i].token_type;
            if (substr_len >= (int)sizeof(tokens[nr_token].str)) {
              printf("token is too long: %.*s\n", substr_len, substr_start);
              return false;
            }
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token ++;
            break;
          }
          default:
            printf("unknown token type: %d\n", rules[i].token_type);
            return false;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  int i;
  for (i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '-' || tokens[i].type == '*') {
      if (i == 0 ||
          !(tokens[i - 1].type == TK_DEC ||
            tokens[i - 1].type == TK_HEX ||
            tokens[i - 1].type == TK_REG ||
            tokens[i - 1].type == ')')) {
        tokens[i].type = (tokens[i].type == '-') ? TK_NEG : TK_DEREF;
      }
    }
  }

  if (nr_token == 0) {
    *success = false;
    return 0;
  }

  *success = true;
  return eval(0, nr_token - 1, success);
}
