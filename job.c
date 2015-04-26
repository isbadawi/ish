#include "job.h"

#include <stdio.h>
#include <stdlib.h>

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
  job->next = shell->stopped_jobs;
  shell->stopped_jobs = job;
  printf("[1]+  Stopped                %s\n", job->command_line);
}

struct ish_process_t *ish_job_process_create(struct ish_job_t *job) {
  struct ish_process_t *proc = malloc(sizeof(*proc));
  if (!proc) {
    return NULL;
  }
  proc->done = 0;
  if (!job->processes) {
    job->processes = proc;
  } else {
    struct ish_process_t *p = job->processes;
    while (p->next) {
      p = p->next;
    }
    p->next = proc;
  }
  proc->next = NULL;
  return proc;
}

struct ish_process_t *ish_job_get_process(struct ish_job_t *job, pid_t pid) {
  FOR_EACH_PROCESS_IN_JOB(job, proc) {
    if (proc->pid == pid) {
      return proc;
    }
  }
  return NULL;
}

int ish_job_done(struct ish_job_t *job) {
  FOR_EACH_PROCESS_IN_JOB(job, proc) {
    if (!proc->done) {
      return 0;
    }
  }
  return 1;
}
