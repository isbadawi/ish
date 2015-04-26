#include "job.h"

#include <stdio.h>
#include <stdlib.h>

#include "list.h"

struct ish_job_t *ish_job_create(void) {
  struct ish_job_t *job = malloc(sizeof(*job));
  if (!job) {
    return NULL;
  }
  job->processes = NULL;
  job->pgid = 0;
  job->stopped = 0;
  return job;
}

void ish_shell_add_stopped_job(struct ish_shell_t *shell, struct ish_job_t *job) {
  job->stopped = 1;
  ISH_LIST_PREPEND(struct ish_job_t*, shell->stopped_jobs, job);
  printf("[1]+  Stopped                %s\n", job->command_line);
}

struct ish_process_t *ish_job_process_create(struct ish_job_t *job) {
  struct ish_process_t *proc = malloc(sizeof(*proc));
  if (!proc) {
    return NULL;
  }
  proc->done = 0;
  ISH_LIST_APPEND(struct ish_process_t *, job->processes, proc);
  proc->next = NULL;
  return proc;
}

struct ish_process_t *ish_job_get_process(struct ish_job_t *job, pid_t pid) {
  ISH_LIST_FOR_EACH(struct ish_process_t*, job->processes, proc) {
    if (proc->pid == pid) {
      return proc;
    }
  }
  return NULL;
}

int ish_job_done(struct ish_job_t *job) {
  ISH_LIST_FOR_EACH(struct ish_process_t*, job->processes, proc) {
    if (!proc->done) {
      return 0;
    }
  }
  return 1;
}
