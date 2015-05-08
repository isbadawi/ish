#include "builtin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "job.h"
#include "list.h"

#define UNUSED __attribute__((unused))

static void ish_builtin_cd(struct ish_shell_t *shell UNUSED, char **args) {
  chdir(args[1] ? args[1] : getenv("HOME"));
}

static void ish_builtin_pwd(struct ish_shell_t *shell UNUSED, char **args UNUSED) {
  char *pwd = getcwd(NULL, 0);
  printf("%s\n", pwd);
  free(pwd);
}

__attribute__((noreturn))
static void ish_builtin_exit(struct ish_shell_t *shell UNUSED, char **args) {
  exit(args[1] ? atoi(args[1]) : 0);
}

static void ish_builtin_export(struct ish_shell_t *shell UNUSED, char **args) {
  for (int i = 1; args[i]; ++i) {
    char *eq = strchr(args[i], '=');
    if (!eq) {
      // TODO(isbadawi): Shell variables
      continue;
    }
    *eq = '\0';
    setenv(args[i], eq + 1, 1);
  }
}

static void ish_builtin_jobs(struct ish_shell_t *shell, char **args UNUSED) {
  int i = 1;
  ISH_LIST_FOR_EACH(struct ish_job_t*, shell->stopped_jobs, job) {
    printf("[%d]+  Stopped                %s\n", i++, job->command_line);
  }
}

static void ish_builtin_fg(struct ish_shell_t *shell, char **args) {
  if (!shell->stopped_jobs) {
    fprintf(stderr, "fg: current: no such job\n");
    return;
  }

  if (!args[1]) {
    struct ish_job_t *job = shell->stopped_jobs;
    tcsetpgrp(STDIN_FILENO, job->pgid);
    kill(-job->pgid, SIGCONT);
  }
}

static struct ish_builtin_t ish_builtins[] = {
  {"cd", ish_builtin_cd},
  {"pwd", ish_builtin_pwd},
  {"exit", ish_builtin_exit},
  {"export", ish_builtin_export},
  {"jobs", ish_builtin_jobs},
  {"fg", ish_builtin_fg},
  {NULL, NULL}
};

struct ish_builtin_t *ish_builtin_get(char *name) {
  for (int i = 0; ish_builtins[i].name; ++i) {
    if (!strcmp(name, ish_builtins[i].name)) {
      return &ish_builtins[i];
    }
  }
  return NULL;
}
