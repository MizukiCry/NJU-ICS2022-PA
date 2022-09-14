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

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

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

static bool is_number(char *s) {
  if (s == NULL) return false;
  if (*s == '-') ++s;
  if (!isdigit(*s)) return false;
  while (*s)
    if (!isdigit(*s++))
      return false;
  return true;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {

  //Fix Error 1
  nemu_state.state = NEMU_QUIT;

  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args) {
  if (args == NULL) {
    cpu_exec(1);
  } else {
    char *first_arg = strtok(args, " ");
    char *other_args = strtok(NULL, " ");
    uint64_t n = 0;
    if (other_args != NULL) {
      printf(ANSI_FMT("Expect exactly one integer.\n", ANSI_FG_RED));
    } else if (*first_arg == '-' || !is_number(first_arg) || (sscanf(first_arg, "%lu", &n), n) == 0) {
      printf(ANSI_FMT("Expect a positive integer.\n", ANSI_FG_RED));
    } else {
      cpu_exec(n);
    }
  }
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf(ANSI_FMT("Expect SUBCMD.\n", ANSI_FG_RED));
  } else {
    char *first_arg = strtok(args, " ");
    char *other_args = strtok(NULL, " ");
    if (other_args != NULL) {
      printf(ANSI_FMT("Expect exactly one SUBCMD.\n", ANSI_FG_RED));
    } else if(strcmp(first_arg, "r") == 0) {
      isa_reg_display();
    } else if(strcmp(first_arg, "w") == 0) {
      printf(ANSI_FMT("To be implemented.\n", ANSI_FG_RED));
    } else {
      printf(ANSI_FMT("Unexpected SUBCMD (expect \"r\" or \"w\").\n", ANSI_FG_RED));
    }
  }
  return 0;
}

static paddr_t calc_expr(char* expr) {

  paddr_t x = 0;
  sscanf(expr, "%u", &x);
  return x;

}

static int cmd_x(char *args) {
  char *first_arg = strtok(args, " ");
  char *second_arg = strtok(NULL, " ");
  char *other_orgs = strtok(NULL, " ");

  paddr_t N = 0, addr = 0;

  if (first_arg == NULL || second_arg == NULL || other_orgs != NULL) {
    printf(ANSI_FMT("Expect an integer N and an expression EXPR.\n", ANSI_FG_RED));
  } else if (!is_number(first_arg) || *first_arg == '-' || (sscanf(first_arg, "%u", &N), N == 0 || N > 1e5)) {
    printf(ANSI_FMT("Expect an positive integer between 0 and 10000.\n", ANSI_FG_GREEN));
    return N;
  } else if (addr = calc_expr(second_arg), addr == 0) {
    printf(ANSI_FMT("Incorrect expression.\n", ANSI_FG_RED));
  } else {
    for (paddr_t i = 0; i < N; ++i) {
      word_t v = paddr_read(addr + i * 4, 4);
      uint8_t *x = (uint8_t *)&v;
      printf("0x%08x : " ANSI_FMT("0x%02x\t0x%02x\t0x%02x\t0x%02x\n", ANSI_FG_GREEN), addr + i * 4, x[0], x[1], x[2], x[3]);
    }
  }
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
  { "info", "(info r/w) Print the status of the program", cmd_info },
  { "x", "(x N EXPR) Print N bytes since address EXPR as an expression", cmd_x },
  /* TODO: Add more commands */

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
