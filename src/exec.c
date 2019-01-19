#include "exec.h"

#include <signal.h>
#include <sys/types.h>

#include "macros.h"


static inline
int
execute (char **argv,
         char **envp)
{
    g_autoptr (GError) err = NULL;
    gint exit_status = -1;
    GSpawnFlags flags = G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL;

    g_spawn_sync (NULL, argv, envp, flags | G_SPAWN_SEARCH_PATH,
                  NULL, NULL, NULL, NULL, &exit_status, &err);
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
    int exit_status = execute (argv, envp);
    g_debug ("Exit status: %d", exit_status);

    // Free envp
    char **e = envp; while (*e) g_free(*e++);

    return exit_status;
}


int
xsus_kill_subtree (pid_t pid,
                   int signal,
                   char *cmd_pattern)
{
    if (! cmd_pattern)
        return 0;

    g_assert (signal == SIGSTOP || signal == SIGCONT);
    char *sig = signal == SIGSTOP ? "STOP" : "CONT";

    // Prefer this short script to using pstree (induces extra dependency)
    // or involved parsing of /proc
    g_autofree char *script = g_strdup_printf (
        "pid=%d\n"
            "ps_output=\"$(ps -e -o ppid,pid,cmd | awk \"/%s/\"'{ print $1, $2 }')\"\n"
            "while [ \"$pid\" ]; do\n"
            "    pid=\"$(echo \"$ps_output\" | awk \"/^($pid) /{ print \\$2 }\")\"\n"
            "    echo -n $pid\" \"\n"
            "    pid=\"$(echo \"$pid\" | paste -sd '|' -)\"\n"
            "done%s | xargs kill -%s 2>/dev/null",
        pid, cmd_pattern, (IS_DEBUG ? " | tee /dev/fd/2" : ""), sig);

    char *argv[] = {
        "sh", "-c", script, NULL};
    char *envp[] = {
        g_strdup_printf ("PATH=%s", g_getenv ("PATH")),
        g_strdup_printf ("LC_ALL=C"),  // Speeds up locale-aware awk
        NULL
    };

    // Execute and return result
    g_debug ("Exec: pstree %d | kill -%s", pid, sig);
    int exit_status = execute (argv, envp);
    g_debug ("Exit status: %d", exit_status);

    // Free envp
    char **e = envp; while (*e) g_free(*e++);

    return exit_status;
}
