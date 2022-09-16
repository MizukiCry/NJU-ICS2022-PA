/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 1,
  TK_L_BRA,
  TK_R_BRA,
  TK_MUL,
  TK_DIV,
  TK_PLUS,
  TK_MINUS,
  TK_NUM,
  TK_EQ,
  TK_NE,
  TK_AND,
  TK_DEREF,     // dereference
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  // Pay attention to the precedence level of different rules.
  {" +", TK_NOTYPE},                    // spaces
  {"\\(", TK_L_BRA},                    // left bracket
  {"\\)", TK_R_BRA},                    // right bracket

  {"\\*", TK_MUL},                      // multiply
  {"/", TK_DIV},                        // divide
  {"\\+", TK_PLUS},                     // plus
  {"-", TK_MINUS},                      // minus

  {"0[xX][0-9a-fA-F]+", TK_NUM},        // hexadecimal integer
  {"[0-9]+", TK_NUM},                   // decimal integer

  {"==", TK_EQ},                        // equal
  {"!=", TK_NE},                        // not equal
  {"&&", TK_AND},                       // and
};

static int pre_lv_info[][3] = {
  {TK_NUM},
  {TK_L_BRA, TK_R_BRA},
  {TK_MUL, TK_DIV},
  {TK_PLUS, TK_MINUS},
  {TK_EQ, TK_NE, TK_AND},
}, pre_lv[64];

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  for (int i = 0; i < sizeof(pre_lv_info) / sizeof(pre_lv_info[0]); ++i)
    for (int j = 0; j < sizeof(pre_lv_info[0]) / sizeof(int); ++j)
      if (pre_lv_info[i][j] != 0) pre_lv[pre_lv_info[i][j]] = i + 1;
      else break;

  char error_msg[128];

  for (int i = 0; i < NR_REGEX; i ++) {
    int ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
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

static Token tokens[1024] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    if (nr_token == sizeof(tokens) / sizeof(Token)) {
      printf(ANSI_FMT("Regex too long.\n", ANSI_FG_RED));
      return false;
    }

    //printf("--? [%d] [(%d)%c]\n", position, (int)e[position], e[position]);

    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        //Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //  i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        if (rules[i].token_type == TK_NOTYPE) break;

        switch (rules[i].token_type) {
          case TK_NUM:
            if (substr_len >= sizeof(tokens[0].str) / sizeof(char)) {
              printf(ANSI_FMT("Regex integer too large.\n", ANSI_FG_RED));
              return false;
            }
            memcpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            break;
          default:
        }
        tokens[nr_token++].type = rules[i].token_type;
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

word_t str_to_num(char* s) {
  word_t res = 0;
  if (*(s + 1) == 'x' || *(s + 1) == 'X')
    sscanf(s, "%i", &res);
  else
    sscanf(s, "%u", &res);
  return res;
}

bool check_parentheses(int p, int q) {
  if (tokens[p].type != TK_L_BRA) return false;
  int t = 0;
  for (int i = p; i <= q; ++i)
  {
    if (tokens[i].type == TK_L_BRA) ++t;
    else if (tokens[i].type == TK_R_BRA) --t;
    if (t < 0) return false;
    if (t == 0 && i != q) return false;
  }
  return t == 0;
}

word_t eval(int p, int q, bool *success) {
  //printf("-- eval %d %d\n", p, q);
  if (p > q) {
    //printf("-- eval false1 %d %d\n", p, q);
    *success = false;
    return 0;
  }
  if (p == q) {
    if (tokens[p].type != TK_NUM) {
      //printf("-- eval false2 %d %d\n", p, q);
      *success = false;
      return 0;
    }
    return str_to_num(tokens[p].str);
  }
  if (p + 1 == q && tokens[p].type == TK_MINUS && tokens[q].type == TK_NUM) {
    return -str_to_num(tokens[q].str);
  }
  if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1, success);
  }
  int main_op = 0, pos = 0, t = 0;
  for (int i = p; i <= q; ++i) {
    if (tokens[i].type == TK_L_BRA) ++t;
    else if (tokens[i].type == TK_R_BRA) --t;
    if (t != 0 || i == p) continue;
    if (pre_lv[tokens[i].type] >= pre_lv[main_op]) {
      main_op = tokens[i].type;
      pos = i;
    }
  }
  if (main_op == 0) {
    //printf("-- eval false3 %d %d\n", p, q);
    *success = false;
    return 0;
  }
  //printf("----(%d|%d) %d %d\n", p, q, main_op, pos);
  word_t lhs = eval(p, pos - 1, success);
  if (!*success) return 0;
  word_t rhs = eval(pos + 1, q, success);
  if (!*success) return 0;
  //printf("----(%d|%d) %u %u\n", p, q, lhs, rhs);
  switch (main_op)
  {
  case TK_PLUS:
    return lhs + rhs;
  case TK_MINUS:
    return lhs - rhs;
  case TK_MUL:
    return lhs * rhs;
  case TK_DIV:
    if (rhs == 0) {
      //printf("-- eval false4 %d %d\n", p, q);
      *success = false;
      return 0;
    }
    return lhs / rhs;
  default:
    //printf("-- eval false5 %d %d\n", p, q);
    *success = false;
  }
  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  /*printf("Made token [%d]\n", nr_token);
  for (int i = 0; i < nr_token; ++i) {
    //if (tokens[i].type != 256)
    printf("-- %d\n", tokens[i].type);
  }*/
  *success = true;
  return eval(0, nr_token - 1, success);
}
