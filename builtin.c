#include "builtin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void ish_builtin_cd(char **args) {
  chdir(args[1] ? args[1] : getenv("HOME"));
}

static void ish_builtin_pwd(char **args) {
  char *pwd = getcwd(NULL, 0);
  printf("%s\n", pwd);
  free(pwd);
}

static void ish_builtin_exit(char **args) {
  exit(args[1] ? atoi(args[1]) : 0);
}

void ish_builtin_export(char **args) {
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

struct ish_builtin_t ish_builtins[] = {
  {"cd", ish_builtin_cd},
  {"pwd", ish_builtin_pwd},
  {"exit", ish_builtin_exit},
  {"export", ish_builtin_export},
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
