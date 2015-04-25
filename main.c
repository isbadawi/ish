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

#include "builtin.h"

char *ish_getline(void) {
  char *line = NULL;
  size_t n = 0;
  getline(&line, &n, stdin);

  // Strip newline...
  line[strlen(line) - 1] = '\0';

  // Strip comments (but not inside quotes)
  int squote = 0;
  int dquote = 0;
  for (int i = 0; line[i]; ++i) {
    if (line[i] == '"' && !squote) {
      dquote = !dquote;
    } else if (line[i] == '\'' && !dquote) {
      squote = !squote;
    } else if (line[i] == '#' && !squote && !dquote) {
      line[i] = '\0';
      break;
    }
  }

  // If it's empty, bail out.
  if (!*line) {
    free(line);
    return NULL;
  }

  // If it's all whitespace, bail out.
  int allspace = 1;
  for (int i = 0; line[i]; ++i) {
    if (!isspace(line[i])) {
      allspace = 0;
      break;
    }
  }
  if (allspace) {
    free(line);
    return NULL;
  }

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

  char **words = cmd->wordexp.we_wordv;
  if (execvp(words[0], words) < 0) {
    perror(words[0]);
    return -1;
  }
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
    struct ish_builtin_t *builtin = ish_builtin_get(tokens[0]);
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
    if (line) {
      ish_eval(line);
      free(line);
    }
  }
  return 0;
}
