#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtin.h"
#include "job.h"
#include "list.h"
#include "parse.h"

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

  execvp(proc->argv[0], proc->argv);
  perror(proc->argv[0]);
  exit(1);
  return -1;
}

void ish_eval_job(struct ish_shell_t *shell, struct ish_job_t *job) {
  // TODO(isbadawi): Builtins & pipes?
  if (!job->processes->next) {
    char **argv = job->processes->argv;
    struct ish_builtin_t *builtin = ish_builtin_get(argv[0]);
    if (builtin) {
      builtin->action(shell, argv);
      return;
    }
  }

  int pipefds[2];
  int writefd;
  int readfd = STDIN_FILENO;
  ISH_LIST_FOR_EACH(struct ish_process_t*, job->processes, proc) {
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
