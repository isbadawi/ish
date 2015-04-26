#include "parse.h"

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

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
  wordexp_t exp;
  while (*command) {
    if (*command == '|') {
      *command = '\0';
      wordexp(cmd, &exp, 0);
      proc->argv = exp.we_wordv;
      cmd = command + 1;
      proc = ish_job_process_create(job);
    }
    command++;
  }
  wordexp(cmd, &exp, 0);
  proc->argv = exp.we_wordv;
  signal(SIGCHLD, SIG_IGN);
}
