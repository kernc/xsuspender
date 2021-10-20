#include "proc.h"
#include <limits.h>
#include <proc/readproc.h>
#include "rule.h"


typedef struct state {
    GTree *procs;
    char *process_name;
    // Types match readproc.h
    int cand_pid;
    unsigned long long cand_start_time;  // NOLINT(runtime/int)
} state;


static
int
compare_ints (gconstpointer a, gconstpointer b, __attribute__((unused)) gpointer data)
{
    return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}


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
gboolean
traverse_procs (__attribute__((unused)) gpointer pid, proc_t *proc, state *state)
{
    int cand_pid = 0;
    unsigned long long cand_start_time = ULLONG_MAX;  // NOLINT(runtime/int)

    for (proc_t *cur = proc; cur; cur = g_tree_lookup(state->procs, GINT_TO_POINTER(cur->ppid))) {
        if (cmdline_matches(cur->cmdline, state->process_name)) {
            cand_pid = cur->tid;
            cand_start_time = cur->start_time;
        }
    }

    if (cand_pid != 0 && state->cand_pid != cand_pid) {
        if (state->cand_pid != 0)
            g_warning("Multiple processes named '%s': %u and %u",
                      state->process_name, state->cand_pid, cand_pid);

        if (cand_start_time < state->cand_start_time ||
            (cand_start_time == state->cand_start_time &&
             cand_pid < state->cand_pid)) {
            // Update
            state->cand_start_time = cand_start_time;
            state->cand_pid = cand_pid;
        }
    }

    return FALSE;
}


static
GTree*
get_processes ()
{
    GTree *result = g_tree_new_full(compare_ints, NULL, NULL, (GDestroyNotify) freeproc);
    PROCTAB  *pt = openproc (PROC_FILLARG | PROC_FILLSTAT);
    proc_t *proc;

    while ((proc = readproc (pt, NULL)))
        g_tree_insert (result, GINT_TO_POINTER(proc->tid), proc);

    closeproc (pt);
    return result;
}


static
int
process_name_get_pid (char *process_name)
{
    GTree *procs = get_processes();

    state state;
    state.procs = procs;
    state.process_name = process_name;
    state.cand_pid = 0;
    state.cand_start_time = ULLONG_MAX;

    g_tree_foreach(procs, (GTraverseFunc) traverse_procs, &state);
    g_tree_destroy (procs);
    return state.cand_pid;
}


int
xsus_window_get_pid (WnckWindow *window)
{
    Rule *rule = xsus_window_get_rule (window);
    char *process_name = rule ? rule->process_name : NULL;
    int pid_by_process_name = process_name ? process_name_get_pid(process_name) : 0;
    // Prefer getting the PID by process name so we can override _NET_WM_PID if it's wrong
    // (e.g. when using flatpak)
    return pid_by_process_name ? pid_by_process_name : wnck_window_get_pid(window);
}
