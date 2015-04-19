#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wordexp.h>

struct ish_builtin_t {
  const char *name;
  void (*action)(char **args);
};

void ish_builtin_cd(char **args) {
  chdir(args[1] ? args[1] : getenv("HOME"));
}

void ish_builtin_pwd(char **args) {
  char *pwd = getcwd(NULL, 0);
  printf("%s\n", pwd);
  free(pwd);
}

void ish_builtin_exit(char **args) {
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

struct ish_builtin_t *ish_get_builtin(char *name) {
  for (int i = 0; ish_builtins[i].name; ++i) {
    if (!strcmp(name, ish_builtins[i].name)) {
      return &ish_builtins[i];
    }
  }
  return NULL;
}

char *ish_getline(void) {
  char *line = NULL;
  size_t n = 0;
  getline(&line, &n, stdin);
  // Strip newline...
  line[strlen(line) - 1] = '\0';
  return line;
}

struct ish_command_t {
  wordexp_t wordexp;
};

struct ish_line_t {
  struct ish_command_t commands[100];
  int ncommands;
};

void ish_shlex(char *command, struct ish_line_t *line) {
  char *cmd = command;
  int i = 0;
  while (*command) {
    if (*command == '|') {
      *command = '\0';
      wordexp(cmd, &line->commands[i++].wordexp, 0);
      cmd = command + 1;
    }
    command++;
  }
  wordexp(cmd, &line->commands[i++].wordexp, 0);
  line->ncommands = i;
}

void set_cloexec(int fd, int on) {
  int flags = fcntl(fd, F_GETFD, 0);
  if (on) {
    flags |= FD_CLOEXEC;
  } else {
    flags &= ~FD_CLOEXEC;
  }
  fcntl(fd, F_SETFD, flags);
}

pid_t ish_spawn(struct ish_command_t *cmd, int readfd, int writefd) {
  pid_t pid = fork();
  if (pid) {
    return pid;
  }

  if (readfd) {
    set_cloexec(readfd, 0);
    dup2(readfd, STDIN_FILENO);
  }

  if (writefd) {
    set_cloexec(writefd, 0);
    dup2(writefd, STDOUT_FILENO);
  }

  execvp(cmd->wordexp.we_wordv[0], cmd->wordexp.we_wordv);
  // Should never get here...
  return 0;
}

void ish_eval(char *command) {
  struct ish_line_t line;
  ish_shlex(command, &line);
  int n = line.ncommands;

  // TODO(isbadawi): Builtins & pipes?
  if (n == 1) {
    char **tokens = line.commands[0].wordexp.we_wordv;
    struct ish_builtin_t *builtin = ish_get_builtin(tokens[0]);
    if (builtin) {
      builtin->action(tokens);
      return;
    }
  }

  int npipes = n - 1;
  int *pipes = malloc(sizeof(int) * npipes * 2);
  for (int i = 0; i < npipes; ++i) {
    pipe(pipes + 2*i);
    set_cloexec(pipes[2*i], 1);
    set_cloexec(pipes[2*i + 1], 1);
  }

  for (int i = 0; i < n; ++i) {
    int readfd = i == 0 ? 0 : pipes[2*i - 2];
    int writefd = i == n - 1 ? 0 : pipes[2*i + 1];
    ish_spawn(&line.commands[i], readfd, writefd);
  }

  for (int i = 0; i < npipes * 2; ++i) {
    close(pipes[i]);
  }

  int status = 0;
  pid_t wait_pid;
  while ((wait_pid = wait(&status)) > 0);
  for (int i = 0; i < n; ++i) {
    wordfree(&line.commands[i].wordexp);
  }
  free(pipes);
}

int main(int argc, char *argv[]) {
  while (1) {
    printf("ish$ ");
    char *line = ish_getline();
    ish_eval(line);
    free(line);
  }
  return 0;
}
