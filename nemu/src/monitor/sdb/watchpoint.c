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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  word_t last;
  char expr[50];
} WP;

static WP wp_pool[NR_WP] = {};
static int wp_use[NR_WP], wp_num;

void new_wp(char* _expr) {
  if (wp_num == NR_WP) {
    printf(ANSI_FMT("The number of watchpoints reached the limit.\n", ANSI_FG_RED));
    return;
  }

  int expr_len = strlen(_expr);
  if (expr_len >= 50) {
    printf(ANSI_FMT("Expression too long.\n", ANSI_FG_RED));
    return;
  }

  bool expr_state;
  word_t expr_val = expr(_expr, &expr_state);
  if (!expr_state) {
    printf(ANSI_FMT("Incorrect expression.\n", ANSI_FG_RED));
    return;
  }

  int p = wp_use[wp_num++];
  wp_pool[p].last = expr_val;
  memcpy(wp_pool[p].expr, _expr, expr_len);
}

void free_wp(int NO) {
  int i;
  for (i = 0; i < wp_num; ++i)
    if (wp_use[i] == NO) {
      int t = wp_use[i];
      wp_use[i] = wp_use[--wp_num];
      wp_use[wp_num] = t;
      printf(ANSI_FMT("Deleted watchpoint [%d]\n", ANSI_FG_GREEN), NO);
      break;
    }
  if (i == wp_num) printf(ANSI_FMT("Can't find watchpoint [%d]\n", ANSI_FG_RED), NO);
}

void init_wp_pool() {
  for (int i = 0; i < NR_WP; i ++) {
    wp_use[i] = i;
  }

}

/* TODO: Implement the functionality of watchpoint */

