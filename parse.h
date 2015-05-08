#ifndef _ish_parse_h_included
#define _ish_parse_h_included

#include <stdio.h>
#include <stddef.h>

struct ish_job_t;

ssize_t ish_getline(FILE *fp, char **line);
void ish_shlex(char *line, struct ish_job_t *job);

#endif
