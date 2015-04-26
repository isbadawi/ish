#ifndef _ish_builtin_h_included
#define _ish_builtin_h_included

struct ish_shell_t;

struct ish_builtin_t {
  const char *name;
  void (*action)(struct ish_shell_t *shell, char **args);
};

struct ish_builtin_t *ish_builtin_get(char *name);

#endif
