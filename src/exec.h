#ifndef XSUSPENDER_SUBPROCESS_H
#define XSUSPENDER_SUBPROCESS_H

#include <sys/types.h>

#include "xsuspender.h"


int xsus_exec_subprocess (char **argv, WindowEntry *entry);
int xsus_kill_subtree (pid_t pid, int signal, char *cmd_pattern);

#endif  // XSUSPENDER_SUBPROCESS_H
