#include "proc.h"
#include <limits.h>
#include <proc/readproc.h>
#include "rule.h"


static
gboolean
cmdline_matches (char **cmdline, char *s)
{
    if (cmdline == NULL)
        return FALSE;

    char *basename = g_path_get_basename (cmdline[0]);
    gboolean result = g_strcmp0 (basename, s) == 0;
    g_free (basename);
    return result;
}


static
proc_t*
proc_by_pid (pid_t pid)
{
    pid_t arr[2] = {pid, 0};
    PROCTAB *pt = openproc(PROC_FILLARG | PROC_FILLSTAT | PROC_PID, arr);
    proc_t *result = readproc(pt, NULL);
    closeproc(pt);
    return result;
}


static
pid_t
process_name_get_pid (char *process_name)
{
    // Types match readproc.c
    pid_t cand_pid = 0;
    unsigned long long cand_start_time = ULLONG_MAX;  // NOLINT(runtime/int)

    PROCTAB *pt = openproc(PROC_FILLARG | PROC_FILLSTAT);
    proc_t *proc;

    while ((proc = readproc(pt, NULL))) {
        pid_t tree_cand_pid = 0;
        unsigned long long tree_cand_start_time = ULLONG_MAX;  // NOLINT(runtime/int)

        // Some applications use multiple processes with the same name -- find the root one
        pid_t ppid;
        do {
            if (cmdline_matches(proc->cmdline, process_name)) {
                tree_cand_pid = proc->tid;
                tree_cand_start_time = proc->start_time;
            }
            ppid = proc->ppid;
            freeproc(proc);
        } while ((proc = proc_by_pid(ppid)));

        if (tree_cand_pid != 0 && tree_cand_pid != cand_pid) {
            if (cand_pid != 0)
                g_warning("Multiple processes named '%s': %u and %u",
                          process_name, cand_pid, tree_cand_pid);

            if (tree_cand_start_time < cand_start_time ||
                (tree_cand_start_time == cand_start_time &&
                 tree_cand_pid < cand_pid)) {
                // Update
                cand_pid = tree_cand_pid;
                cand_start_time = tree_cand_start_time;
            }
        }
    }

    closeproc(pt);
    return cand_pid;
}


pid_t
xsus_window_get_pid (WnckWindow *window)
{
    Rule *rule = xsus_window_get_rule (window);
    char *process_name = rule ? rule->process_name : NULL;
    pid_t pid_by_process_name = process_name ? process_name_get_pid(process_name) : 0;
    // Prefer getting the PID by process name so we can override _NET_WM_PID if it's wrong
    // (e.g. when using flatpak)
    return pid_by_process_name ? pid_by_process_name : wnck_window_get_pid(window);
}
