#ifndef _ish_job_h_included
#define _ish_job_h_included

#include <wordexp.h>
#include <sys/types.h>

struct ish_process_t {
  char **argv;
  pid_t pid;
  int done;

  struct ish_process_t *next;
};

struct ish_job_t {
  struct ish_process_t *processes;

  char *command_line;
  pid_t pgid;
  int stopped;

  struct ish_job_t *next;
};

struct ish_shell_t {
  struct ish_job_t *stopped_jobs;
};

struct ish_job_t *ish_job_create(void);
void ish_shell_add_stopped_job(struct ish_shell_t *shell, struct ish_job_t *job);
struct ish_process_t *ish_job_process_create(struct ish_job_t *job);
struct ish_process_t *ish_job_get_process(struct ish_job_t *job, pid_t pid);
int ish_job_done(struct ish_job_t *job);

#endif
