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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <memory/paddr.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();
void new_wp(char*);
void free_wp(int);
void print_wp_state();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args) {
  if (args == NULL) {
    cpu_exec(1);
  } else {
    bool n_status;
    word_t n = expr(args, &n_status);
    if (!n_status) {
      printf(ANSI_FMT("Incorrect expression.\n", ANSI_FG_RED));
    } else {
      cpu_exec(n);
    }
  }
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf(ANSI_FMT("Expect an argument.\n", ANSI_FG_RED));
  } else if(strcmp(args, "r") == 0) {
    isa_reg_display();
  } else if(strcmp(args, "w") == 0) {
    print_wp_state();
  } else {
    printf(ANSI_FMT("Wrong argument(expect \"r\" or \"w\").\n", ANSI_FG_RED));
  }
  return 0;
}

static int cmd_x(char *args) {
  char *first_arg = strtok(args, " ");
  char *second_arg = strtok(NULL, " ");
  char *other_orgs = strtok(NULL, " ");

  bool n_state, addr_state;
  paddr_t n = expr(first_arg, &n_state), addr = expr(second_arg, &addr_state);

  if (first_arg == NULL || second_arg == NULL || other_orgs != NULL) {
    printf(ANSI_FMT("Expect an integer N and an expression EXPR.\n", ANSI_FG_RED));
  } else if (!n_state || n == 0 || n > 1e3) {
    printf(ANSI_FMT("Expect an positive integer between 1 and 1000.\n", ANSI_FG_GREEN));
    return n;
  } else if (!addr_state) {
    printf(ANSI_FMT("Incorrect expression.\n", ANSI_FG_RED));
  } else {
    for (paddr_t i = 0; i < n; ++i) {
      word_t v = paddr_read(addr + i * 4, 4);
      uint8_t *x = (uint8_t *)&v;
      printf("0x%08x : " ANSI_FMT("0x%02x    0x%02x    0x%02x    0x%02x\n", ANSI_FG_GREEN), addr + i * 4, x[0], x[1], x[2], x[3]);
    }
  }
  return 0;
}

static int cmd_p(char* args) {
  bool res_state;
  word_t res = expr(args, &res_state);
  if (!res_state) {
    printf(ANSI_FMT("Incorrect expression.\n", ANSI_FG_RED));
  } else {
    printf(ANSI_FMT("[%u]\n", ANSI_FG_GREEN), res);
  }
  return 0;
}

static int cmd_w(char* args) {
  new_wp(args);
  return 0;
}

static int cmd_d(char* args) {
  bool n_state;
  word_t n = expr(args, &n_state);
  if (!n_state) printf(ANSI_FMT("Incorrect expression.\n", ANSI_FG_RED));
  else free_wp(n);
  return 0;
}

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "(si [N]) Execute N(1 by default) instructions in single step and then pause it", cmd_si},
  { "info", "(info r/w) Print the status of registers/watchpoints", cmd_info },
  { "x", "(x N EXPR) Print N bytes since address EXPR as an expression", cmd_x },
  { "p", "(p EXPR) Print the result of an expression", cmd_p },
  { "w" ,"(w EXPR) Set a new watchpoint, when the value of w changed, pause the program", cmd_w },
  { "d", "(d N) Delete the watchpoint with number N", cmd_d },
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
