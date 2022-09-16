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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
#define BUFLEN 65536
#define TARLEN 5000   // smaller than 'BUFLEN'
static char buf[BUFLEN] = {};
static char buf_u[BUFLEN] = {};
static char code_buf[BUFLEN + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int pos, pos_u;

static int choose(int x) {
  return rand() % x;
}

static void append_char(char c) {
  if (pos_u > TARLEN) {
    printf("[WARNING]Buffer overflow!\n");
    assert(0);
  }
  buf[pos++] = buf_u[pos_u++] = c;
}

static void append_str(char *s) {
  while(*s) append_char(*s++);
}

static void gen_rand_space() {
  for (int i = choose(3); i >= 0; --i)
    append_char(' ');
}

static void gen_rand_num() {
  strcpy(buf_u + pos_u, "(unsigned int)");
  pos_u += 14;
  append_char('1' + choose(9));
  for (int i = choose(5); i >= 0; --i)
    append_char('0' + choose(10));
}

static void gen_rand_expr() {
  gen_rand_space();
  if (pos_u > TARLEN / 2) {
    gen_rand_num();
  } else {
    switch (choose(3))
    {
    case 0:
      gen_rand_num();
      break;
    case 1:
      append_char('(');
      gen_rand_expr();
      append_char(')');
      break;
    default:
      char op = "+-*/"[choose(4)];
      switch (op)
      {
      case '/':
        gen_rand_expr();
        append_str("/((");
        gen_rand_expr();
        append_str(")*0+");
        gen_rand_num();
        append_char(')');
        break;
      default:
        gen_rand_expr();
        append_char(op);
        gen_rand_expr();
      }
    }
  }
  gen_rand_space();
}

static void start_gen() {
  pos = pos_u = 0;
  gen_rand_expr();
  append_char('\0');
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    //gen_rand_expr();
    fprintf(stderr, "Generating expr [%d/%d]\n", i + 1, loop);
    start_gen();

    //printf("[DEBUG]%s\n", buf_u);

    sprintf(code_buf, code_format, buf_u);

    //printf("[DEBUG]%s\n", code_buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    seed = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
