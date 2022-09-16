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

#include <common.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

word_t expr(char *e, bool *success);

int main(int argc, char *argv[]) {
  /* Initialize the monitor. */
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif

  //init_regex();
  FILE* f = fopen("tools/gen-expr/build/input", "r");
  assert(f != NULL);
  uint32_t x;
  bool x_state;
  char _expr[65536];
  while (~fscanf(f, "%u%[^\n]", &x, _expr)) {
    uint32_t res = expr(_expr, &x_state);
    assert(x_state);
    assert(x == res);
    printf("Success");
  }
  fclose(f);
  return 0;

  /* Start engine. */
  engine_start();

  return is_exit_status_bad();
}
