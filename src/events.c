#include "events.h"

#include <glib.h>
#include <libwnck/libwnck.h>

#include <sys/param.h>
#include <time.h>

#include "entry.h"
#include "macros.h"


void
xsus_init_event_handlers ()
{
    WnckScreen *screen = wnck_screen_get_default ();

    g_signal_connect (screen, "active-window-changed",
                      G_CALLBACK (on_active_window_changed), NULL);

    // Periodically run handler tending to the suspension queue
    g_timeout_add_seconds_full (
        G_PRIORITY_LOW, SUSPEND_PENDING_INTERVAL, on_suspend_pending_windows, NULL, NULL);

    // Periodically resume windows for a while so they get "up to date"
    g_timeout_add_seconds_full (
        G_PRIORITY_LOW, PERIODIC_RESUME_INTERVAL,  on_periodic_window_wake_up, NULL, NULL);

    // Periodically check if we're on battery power
    g_timeout_add_seconds_full (
        G_PRIORITY_LOW, SLOW_INTERVAL, on_check_battery_powered, NULL, NULL);
}


static
WnckWindow*
get_main_window (WnckWindow *window)
{
    if (! WNCK_IS_WINDOW (window))
        return NULL;

    // Resolve transient (dependent, dialog) windows
    WnckWindow *parent;
    while ((parent = wnck_window_get_transient (window)))
        window = parent;

    // Ensure window is of correct type
    WnckWindowType type = wnck_window_get_window_type (window);
    if (type == WNCK_WINDOW_NORMAL ||
        type == WNCK_WINDOW_DIALOG)
        return window;

    return NULL;
}


static inline
gboolean
windows_are_same_process (WnckWindow *w1,
                          WnckWindow *w2)
{
    // Consider windows to be of the same process when they
    // are one and the same window,
    if (w1 == w2)
        return TRUE;

    // Or when they have the same PID, map to the same rule,
    // and the rule says that signals should be sent.
    Rule *rule;
    return (WNCK_IS_WINDOW (w1) &&
            WNCK_IS_WINDOW (w2) &&
            wnck_window_get_pid (w1) == wnck_window_get_pid (w2) &&
            (rule = xsus_window_get_rule (w1)) &&
            rule->send_signals &&
            rule == xsus_window_get_rule (w2));
}


void
on_active_window_changed (WnckScreen *screen,
                          WnckWindow *prev_active_window)
{
    WnckWindow *active_window = wnck_screen_get_active_window (screen);

    active_window = get_main_window (active_window);
    prev_active_window = get_main_window (prev_active_window);

    // Main windows are one and the same; do nothing
    if (windows_are_same_process (active_window, prev_active_window))
        return;

    // Resume the active window if it was (to be) suspended
    if (active_window)
        xsus_window_resume (active_window);

    // Maybe suspend previously active window
    if (prev_active_window)
        xsus_window_suspend (prev_active_window);
}


static inline
pid_t
window_entry_get_pid (WindowEntry *entry)
{
    WnckWindow *window = wnck_window_get (entry->xid);
    return window && wnck_window_get_pid (window) == entry->pid ? entry->pid : 0;
}


int
on_suspend_pending_windows ()
{
    time_t now = time (NULL);
    GSList *l = queued_entries;
    while (l) {
        GSList *next = l->next;
        WindowEntry *entry = l->data;

        if (now >= entry->suspend_timestamp) {
            queued_entries = g_slist_delete_link (queued_entries, l);

            // Follow through with suspension.
            // This is safe even without ensuring `window_entry_get_pid (entry)`
            // because the OS simply won't will invalid PIDs, and we let the
            // exec_suspend= scripts run.
            // This fixes a bug where the app window minimizes to system tray,
            // making `wnck_window_get_pid` return invalid PID for an obviously
            // non-existent window, but the process/PID is still there,
            // letting us kill it.
            // In this configuration with systray, it is important to have
            // low resume_every= values to be able to get the iconified app back up!
            xsus_signal_stop (entry);
        }
        l = next;
    }
    return TRUE;
}


int
on_periodic_window_wake_up ()
{
    time_t now = time (NULL);
    GSList *l = suspended_entries;
    while (l) {
        WindowEntry *entry = l->data;
        l = l->next;

        // Is it time to resume the process?
        if (entry->rule->resume_every &&
            now - entry->suspend_timestamp >= entry->rule->resume_every) {
            g_debug ("Periodic awaking %#lx (%d) for %d seconds",
                     entry->xid, entry->pid, entry->rule->resume_for);

            // Re-schedule suspension if window is still alive
            if (window_entry_get_pid (entry)) {
                // Make a copy because continuing below frees the entry
                WindowEntry *copy = xsus_window_entry_copy (entry);
                xsus_window_entry_enqueue (copy, entry->rule->resume_for);
            }

            xsus_signal_continue (entry);
        }
    }
    return TRUE;
}


static inline
Rule*
main_window_get_rule (WnckWindow *window)
{
    return window == get_main_window (window) ? xsus_window_get_rule (window) : NULL;
}


static
void
iterate_windows_kill_matching ()
{
    WnckScreen *screen = wnck_screen_get_default ();
    WnckWindow *active = wnck_screen_get_active_window (screen);

    for (GList *w = wnck_screen_get_windows (screen); w ; w = w->next) {
        WnckWindow *window = w->data;
        Rule *rule = main_window_get_rule (window);

        // Skip non-matching windows
        if (! rule)
            continue;

        // Skip currently focused window
        if (active != NULL && windows_are_same_process (window, active))
            continue;

        // On battery, auto-suspend windows that allow it
        if (is_battery_powered && rule->auto_on_battery) {
            // Do nothing if we're already keeping track of this window
            if (xsus_entry_find_for_window_rule (window, rule, queued_entries) ||
                xsus_entry_find_for_window_rule (window, rule, suspended_entries))
                continue;

            // Otherwise, schedule the window for suspension shortly
            WindowEntry *entry = xsus_window_entry_new (window, rule);
            xsus_window_entry_enqueue (entry, rule->resume_for);

        // On AC, don't auto-resume windows that want to be suspended also
        // when on AC, e.g. VirtualBox with Windos
        } else if (!is_battery_powered && rule->only_on_battery) {
            xsus_window_resume (window);
        }
    }
}


static
gboolean
is_on_ac_power ()
{
#ifdef __linux__
    // Read AC power state from /sys/class/power_supply/*/online (== 1 on AC).
    // Should work in most cases. See: https://bugs.debian.org/473629

    const char *DIRNAME = "/sys/class/power_supply";
    const char *basename;
    g_autoptr (GError) err = NULL;
    g_autoptr (GDir) dir = g_dir_open (DIRNAME, 0, &err);

    if (err) {
        g_warning ("Cannot read battery/AC status: %s", err->message);
        return FALSE;
    }

    while ((basename = g_dir_read_name (dir))) {
        if (g_str_has_prefix (basename, "hid"))
            continue;  // Skip HID devices, GH-38

        g_autofree char *filename = g_build_filename (DIRNAME, basename, "online", NULL);
        g_autofree char *contents = NULL;

        if (! g_file_get_contents (filename, &contents, NULL, NULL))
            continue;

        if (g_strcmp0 (g_strstrip (contents), "1") == 0)
            return TRUE;
    }
    return FALSE;

#elif defined(__unix__) && defined(BSD) && !defined(__APPLE__)
    // On *BSD, run `apm -a` which returns '1' when AC is online
    g_autoptr (GError) err = NULL;
    g_autofree char *standard_output = NULL;
    char *argv[] = {"apm", "-a", NULL};

    g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, &standard_output, NULL, NULL, &err);
    if (err)
        g_warning ("Unexpected `apm -a` execution error: %s", err->message);

    return standard_output && 0 == g_strcmp0 (g_strstrip (standard_output), "1");

#else
    #warning "No battery / AC status support for your platform."
    #warning "Defaulting to as if 'always on battery' behavior. Patches welcome!"
    return FALSE;
#endif
}


static inline
gboolean
any_rule_downclocks ()
{
    for (int i = 0; rules[i]; ++i)
        if (rules[i]->downclock_on_battery)
            return TRUE;
    return FALSE;
}


static void stop_downclocking ();


int
on_check_battery_powered ()
{
    gboolean previous_state = is_battery_powered;
    is_battery_powered = ! is_on_ac_power ();

    // On battery state change, suspend / resume matching windows
    if (previous_state != is_battery_powered) {
        g_debug ("AC power = %d; State changed. Suspending/resuming windows.",
                 ! is_battery_powered);
        iterate_windows_kill_matching ();

        // If downclocking is enabled, also start/stop doing that
        if (any_rule_downclocks()) {
            if (is_battery_powered) {
                g_timeout_add_full (
                    G_PRIORITY_HIGH_IDLE, FAST_INTERVAL_MSEC, on_downclock_slice, NULL, NULL);

                g_timeout_add_seconds_full (
                    G_PRIORITY_LOW, SLOW_INTERVAL, on_update_downclocked_processes, NULL, NULL);

                on_update_downclocked_processes ();
            } else{
                stop_downclocking ();
            }
        }
    }
    return TRUE;
}


typedef struct DownclockPair {
    WindowEntry *entry;
    guint32 counter;
} DownclockPair;

// Downclocked processes (configured with downclock_on_battery > 0) are
// periodically sent STOP and CONT in short time slices. These two lists
// of DownclockPair track them.
static GList *downclock_suspended = NULL;
static GSList *downclock_running = NULL;

// Simple counter of allocated time slices; avoids querying sub-second time
static guint32 downclock_slice_counter = 0;


static inline
DownclockPair*
downclock_pair_new (WnckWindow *window,
                    Rule *rule)
{
    DownclockPair *pair = g_malloc (sizeof (DownclockPair));
    pair->entry = xsus_window_entry_new (window, rule);
    pair->counter = downclock_slice_counter;
    return pair;
}


int
on_downclock_slice ()
{
    if (! is_battery_powered)
        return FALSE;

    downclock_slice_counter ++;

    // Suspend processes that have ran for the past time slice
    for (GSList *l = downclock_running; l; l = l->next) {
        DownclockPair *pair = l->data;
        WindowEntry *entry = pair->entry;

        pid_t pid = window_entry_get_pid (entry);
        if (! pid)
            continue;

        // Suspend the process
        kill (pid, SIGSTOP);

        // Signal to raise after so-many time slices
        pair->counter = downclock_slice_counter + entry->rule->downclock_on_battery;

        // Put in "suspended" queue
        downclock_suspended = g_list_prepend (downclock_suspended, pair);
    }
    g_slist_free (downclock_running);
    downclock_running = NULL;

    // Resume processes that have been sleeping for the past few time slices
    GList *l = downclock_suspended;
    while (l) {
        GList *next = l->next;
        DownclockPair *pair = l->data;

        if (downclock_slice_counter >= pair->counter) {
            downclock_suspended = g_list_delete_link (downclock_suspended, l);
            WindowEntry *entry = pair->entry;

            pid_t pid = window_entry_get_pid (entry);
            if (! pid)
                continue;

            // Only downclock-resume the current process if it's not already
            // fully suspended by the other mechanism
            WnckWindow *window = wnck_window_get (entry->xid);
            if (! entry->rule->send_signals ||
                ! xsus_entry_find_for_window_rule (window, entry->rule, suspended_entries))
                kill (pid, SIGCONT);

            downclock_running = g_slist_prepend (downclock_running, pair);
        }
        l = next;
    }
    return TRUE;
}


int
on_update_downclocked_processes ()
{
    if (! is_battery_powered)
        return FALSE;

    // A set of PIDs of already downclocked processes
    g_autoptr (GHashTable) old_pids = g_hash_table_new (g_direct_hash, g_direct_equal);
    // A set of PIDs that will become downclocked in the current function invocation
    g_autoptr (GHashTable) new_pids = g_hash_table_new (g_direct_hash, g_direct_equal);

    // Fill the set of known PIDs
    for (GSList *l = downclock_running; l; l = l->next)
        g_hash_table_add (old_pids, GINT_TO_POINTER (((DownclockPair*) l->data)->entry->pid));
    for (GList *l = downclock_suspended; l; l = l->next)
        g_hash_table_add (old_pids, GINT_TO_POINTER (((DownclockPair*) l->data)->entry->pid));

    // Iterate over all windows and find processes to downclock
    for (GList *w = wnck_screen_get_windows (wnck_screen_get_default ()); w ; w = w->next) {
        WnckWindow *window = w->data;
        Rule *rule = main_window_get_rule (window);

        // Skip non-matching windows
        if (! rule || ! rule->downclock_on_battery)
            continue;

        // Skip any windows/PIDs we already know about
        pid_t pid = wnck_window_get_pid (window);
        if (g_hash_table_contains (old_pids, GINT_TO_POINTER (pid)) ||
            g_hash_table_contains (new_pids, GINT_TO_POINTER (pid)))
            continue;
        g_hash_table_add (new_pids, GINT_TO_POINTER (pid));

        // Begin downclocking the process
        DownclockPair *pair = downclock_pair_new (window, rule);
        g_debug ("Downclocking %#lx (%d): %s",
                 pair->entry->xid, pair->entry->pid, pair->entry->wm_name);
        downclock_running = g_slist_prepend (downclock_running, pair);
    }
    return TRUE;
}


static
void
stop_downclocking ()
{
    // Resume downclocked processes
    for (GList *l = downclock_suspended; l; l = l->next) {
        WindowEntry *entry = ((DownclockPair*) l->data)->entry;
        g_debug ("Normal-clocking %#lx (%d): %s",
                 entry->xid, entry->pid, entry->wm_name);
        kill (entry->pid, SIGCONT);
    }

    // Free entries
    for (GSList *l = downclock_running; l; l = l->next)
        xsus_window_entry_free (((DownclockPair*) l->data)->entry);
    for (GList *l = downclock_suspended; l; l = l->next)
        xsus_window_entry_free (((DownclockPair*) l->data)->entry);
    g_slist_free (downclock_running);
    g_list_free (downclock_suspended);
    downclock_running = NULL;
    downclock_suspended = NULL;
}


void
xsus_exit_event_handlers ()
{
    stop_downclocking ();
}
