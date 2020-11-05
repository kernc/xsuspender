#include "exec.h"

#include <string.h>

#include <signal.h>
#include <sys/types.h>

#include "macros.h"


static inline
int
execute (char **argv,
         char **envp,
         char **stdout)
{
    g_autoptr (GError) err = NULL;
    gint exit_status = -1;
    GSpawnFlags flags = G_SPAWN_STDERR_TO_DEV_NULL |
                        (stdout ? G_SPAWN_DEFAULT : G_SPAWN_STDOUT_TO_DEV_NULL);

    g_spawn_sync (NULL, argv, envp, flags | G_SPAWN_SEARCH_PATH,
                  NULL, NULL, stdout, NULL, &exit_status, &err);
    if (err)
        g_warning ("Unexpected subprocess execution error: %s", err->message);

    return exit_status;
}


int
xsus_exec_subprocess (char **argv,
                      WindowEntry *entry)
{
    if (! argv)
        return 0;

    // Provide window data to subprocess via the environment
    char *envp[] = {
        g_strdup_printf ("PID=%d", entry->pid),
        g_strdup_printf ("XID=%#lx", entry->xid),
        g_strdup_printf ("WM_NAME=%s", entry->wm_name),
        g_strdup_printf ("PATH=%s", g_getenv ("PATH")),
        g_strdup_printf ("LC_ALL=C"),  // Speeds up locale-aware shell utils
        NULL
    };

    // Execute and return result
    g_debug ("Exec %#lx (%d): %s", entry->xid, entry->pid, argv[2]);
    int exit_status = execute (argv, envp, NULL);
    g_debug ("Exit status: %d", exit_status);

    // Free envp
    char **e = envp; while (*e) g_free(*e++);

    return exit_status;
}


static
void
kill_recursive (char* pid_str, int signal, char* cmd_pattern)
{
    // Pgrep children
    char *argv[] = {"pgrep", "-fP", pid_str, cmd_pattern, NULL};
    g_autofree char *standard_output = NULL;
    execute (argv, NULL, &standard_output);
    if (! standard_output || 0 == strlen(g_strstrip (standard_output)))
        return;  // No children

    standard_output = g_strdelimit(standard_output, "\n", ' ');
    g_debug ("      kill -%s %s", signal == SIGSTOP ? "STOP" : "CONT", standard_output);

    // Kill children and recurse
    g_auto (GStrv) child_pids = g_strsplit (standard_output, " ", 0);
    for (int i = 0; child_pids[i]; ++i) {
        pid_t child_pid = g_ascii_strtoll (child_pids[i], NULL, 10);
        kill (child_pid, signal);
        kill_recursive (child_pids[i], signal, cmd_pattern);
    }
}


void
xsus_kill_subtree (pid_t pid,
                   int signal,
                   char *cmd_pattern)
{
    if (! cmd_pattern)
        return;

    g_assert (signal == SIGSTOP || signal == SIGCONT);

    g_debug ("Exec: pstree %d (%s) | kill -%s",
             pid, cmd_pattern, signal == SIGSTOP ? "STOP" : "CONT");
    g_debug ("      kill -%s %d", signal == SIGSTOP ? "STOP" : "CONT", pid);
    kill (pid, signal);

    char pid_str[11];
    g_snprintf ((char*) &pid_str, sizeof (pid_str), "%d", pid);
    kill_recursive (pid_str, signal, cmd_pattern);
}
