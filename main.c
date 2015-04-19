#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

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

struct ish_builtin_t ish_builtins[] = {
  {"cd", ish_builtin_cd},
  {"pwd", ish_builtin_pwd},
  {"exit", ish_builtin_exit},
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
  line[n - 2] = '\0';
  return line;
}

struct ish_command_t {
  char *tokens[100];
};

struct ish_line_t {
  struct ish_command_t commands[100];
  int ncommands;
};

void ish_shlex(char *command, struct ish_line_t *line) {
  line->ncommands = 1;
  int i = 0;
  int j = 0;
  struct ish_command_t *cmd = &line->commands[j++];
  cmd->tokens[i++] = command;
  int space = 0;
  while (*command) {
    if (*command == '|') {
      line->ncommands ++;
      cmd = &line->commands[j++];
      i = 0;
    } else if (isspace(*command)) {
      *command = '\0';
      space = 1;
    } else if (space) {
      cmd->tokens[i++] = command;
      space = 0;
    }
    command++;
  }
  cmd->tokens[i++] = NULL;
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

  execvp(cmd->tokens[0], cmd->tokens);
  // Should never get here...
  return 0;
}

void ish_eval_line(struct ish_line_t *line) {
  int n = line->ncommands;

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
    ish_spawn(&line->commands[i], readfd, writefd);
  }

  for (int i = 0; i < npipes * 2; ++i) {
    close(pipes[i]);
  }

  int status = 0;
  pid_t wait_pid;
  while ((wait_pid = wait(&status)) > 0);
  free(pipes);
}

void ish_eval(char *command) {
  struct ish_line_t line;
  ish_shlex(command, &line);

  ish_eval_line(&line);
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
