#ifndef _ish_parse_h_included
#define _ish_parse_h_included

#include <stdio.h>

struct ish_job_t;

int ish_getline(FILE *fp, char **line);
void ish_shlex(char *line, struct ish_job_t *job);

#endif
