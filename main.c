#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wordexp.h>

#include "builtin.h"
#include "job.h"

int ish_getline(FILE *fp, char **line) {
  size_t n = 0;
  *line = NULL;
  int rc = getline(line, &n, fp);
  if (rc < 0) {
    return rc;
  }

  // Strip newline...
  (*line)[strlen(*line) - 1] = '\0';

  // Strip comments (but not inside quotes)
  int squote = 0;
  int dquote = 0;
  for (int i = 0; (*line)[i]; ++i) {
    if ((*line)[i] == '"' && !squote) {
      dquote = !dquote;
    } else if ((*line)[i] == '\'' && !dquote) {
      squote = !squote;
    } else if ((*line)[i] == '#' && !squote && !dquote) {
      (*line)[i] = '\0';
      break;
    }
  }

  // If it's empty, bail out.
  if (!**line) {
    free(*line);
    *line = NULL;
    return 0;
  }

  // If it's all whitespace, bail out.
  int allspace = 1;
  for (int i = 0; (*line)[i]; ++i) {
    if (!isspace((*line)[i])) {
      allspace = 0;
      break;
    }
  }
  if (allspace) {
    free(*line);
    *line = NULL;
  }

  return 0;
}

void ish_shlex(char *command, struct ish_job_t *job) {
  job->command_line = command;
  // wordexp fails if SIGCHLD is ignored...
  signal(SIGCHLD, SIG_DFL);
  char *cmd = command;
  struct ish_process_t *proc = ish_job_process_create(job);
  while (*command) {
    if (*command == '|') {
      *command = '\0';
      wordexp(cmd, &proc->wordexp, 0);
      cmd = command + 1;
      proc = ish_job_process_create(job);
    }
    command++;
  }
  wordexp(cmd, &proc->wordexp, 0);
  signal(SIGCHLD, SIG_IGN);
}

void set_cloexec(int fd) {
  int flags = fcntl(fd, F_GETFD, 0);
  flags |= FD_CLOEXEC;
  fcntl(fd, F_SETFD, flags);
}

pid_t ish_spawn(struct ish_process_t *proc, pid_t pgid,
                int readfd, int writefd) {
  pid_t pid = fork();
  if (pid) {
    return pid;
  }

  if (isatty(fileno(stdin))) {
    pid = getpid();
    if (pgid == 0) {
      pgid = pid;
    }
    setpgid(pid, pgid);
    tcsetpgrp(STDIN_FILENO, pgid);

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
  }

  if (readfd != STDIN_FILENO) {
    dup2(readfd, STDIN_FILENO);
    close(readfd);
  }

  if (writefd != STDOUT_FILENO) {
    dup2(writefd, STDOUT_FILENO);
    close(writefd);
  }

  char **words = proc->wordexp.we_wordv;
  execvp(words[0], words);
  perror(words[0]);
  exit(1);
  return -1;
}

void ish_eval_job(struct ish_shell_t *shell, struct ish_job_t *job) {
  // TODO(isbadawi): Builtins & pipes?
  if (!job->processes->next) {
    char **tokens = job->processes->wordexp.we_wordv;
    struct ish_builtin_t *builtin = ish_builtin_get(tokens[0]);
    if (builtin) {
      builtin->action(shell, tokens);
      return;
    }
  }

  int pipefds[2];
  int writefd;
  int readfd = STDIN_FILENO;
  FOR_EACH_PROCESS_IN_JOB(job, proc) {
    if (proc->next) {
      pipe(pipefds);
      set_cloexec(pipefds[0]);
      set_cloexec(pipefds[1]);
      writefd = pipefds[1];
    } else {
      writefd = STDOUT_FILENO;
    }
    proc->pid = ish_spawn(proc, job->pgid, readfd, writefd);
    if (job->pgid == 0) {
      job->pgid = proc->pid;
    }
    if (readfd != STDIN_FILENO) {
      close(readfd);
    }
    if (writefd != STDOUT_FILENO) {
      close(writefd);
    }
    readfd = pipefds[0];
  }

  int status = 0;
  pid_t wait_pid;
  do {
    if ((wait_pid = waitpid(WAIT_ANY, &status, WUNTRACED)) < 0) {
      break;
    }
    if (WIFSTOPPED(status)) {
      ish_shell_add_stopped_job(shell, job);
    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
      struct ish_process_t *proc = ish_job_get_process(job, wait_pid);
      wordfree(&proc->wordexp);
      proc->done = 1;
    }
  } while (!job->stopped && !ish_job_done(job));

  tcsetpgrp(STDIN_FILENO, getpid());
}

// Job control stuff. Reference:
// https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.3/html_chapter/libc_27.html
void ish_init_job_control(void) {
  pid_t pgid;
  // Loop until we're in the foreground...
  while (tcgetpgrp(STDIN_FILENO) != (pgid = getpgrp())) {
    // n.b. -pid means send signal to all processes in the process group.
    kill(-pgid, SIGTTIN);
  }

  // Ignore job control signals...
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

  // Create new process group and place into foreground...
  pgid = getpid();
  if (setpgid(pgid, pgid) < 0) {
    perror("setpgid");
    exit(1);
  }
  tcsetpgrp(STDIN_FILENO, pgid);
}

void ish_eval_line(struct ish_shell_t *shell, char *line) {
  struct ish_job_t *job = ish_job_create();
  ish_shlex(line, job);
  ish_eval_job(shell, job);
}

int main(int argc, char *argv[]) {
  struct ish_shell_t shell = {0};

  int opt;
  while ((opt = getopt(argc, argv, "c:")) != -1) {
    switch (opt) {
    case 'c':
      ish_eval_line(&shell, optarg);
      return 0;
    case '?':
      return 1;
    }
  }

  FILE *fp = stdin;
  if (optind < argc) {
    fp = fopen(argv[optind], "r");
    if (!fp) {
      perror(argv[optind]);
      return 1;
    }
  }

  int interactive = fp == stdin && isatty(fileno(stdin));

  char *line;
  if (interactive) {
    ish_init_job_control();
    printf("ish$ ");
  }
  while (ish_getline(fp, &line) >= 0) {
    if (line) {
      ish_eval_line(&shell, line);
      free(line);
    }
    if (interactive) {
      printf("ish$ ");
    }
  }
  return 0;
}
