#include "events.h"

#include <glib.h>

#include <libwnck/libwnck.h>
#include <time.h>

#include "entry.h"


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
        G_PRIORITY_LOW, CHECK_BATTERY_INTERVAL, on_check_battery_powered, NULL, NULL);
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
gboolean
window_exists (WindowEntry *entry)
{
    WnckWindow *window = wnck_window_get (entry->xid);
    return window && wnck_window_get_pid (window) == entry->pid;
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

            // Follow through with suspension only if window is still alive
            if (window_exists (entry))
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
            if (window_exists (entry)) {
                // Make a copy because continuing below frees the entry
                WindowEntry *copy = xsus_window_entry_copy (entry);
                xsus_window_entry_enqueue (copy, entry->rule->resume_for);
            }

            xsus_signal_continue (entry);
        }
    }
    return TRUE;
}


static
void
iterate_windows_kill_matching ()
{
    for (GList *w = wnck_screen_get_windows (wnck_screen_get_default ()); w ; w = w->next) {
        WnckWindow *window = w->data;

        // Skip transient windows and windows of incorrect type
        if (window != get_main_window (window))
            continue;

        Rule *rule = xsus_window_get_rule (window);

        // Skip non-matching windows
        if (! rule)
            continue;

        // Skip currently focused window
        if (wnck_window_is_active (window))
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
    int exit_status = -1;
    // Read AC power state. Should work in most cases. See: https://bugs.debian.org/473629
    char *argv[] = {"sh", "-c", "grep -q 1 /sys/class/power_supply/*/online", NULL};
    g_spawn_sync (NULL, argv, NULL,
                  G_SPAWN_SEARCH_PATH | (IS_DEBUG ? G_SPAWN_DEFAULT : G_SPAWN_STDERR_TO_DEV_NULL),
                  NULL, NULL, NULL, NULL, &exit_status, NULL);
    gboolean is_ac_power = exit_status == 0;
    return is_ac_power;
#else
    #warning "No battery / AC status support for your platform."
    #warning "Defaulting to as if 'always on battery' behavior. Patches welcome!"
    return FALSE;
#endif
}


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
    }
    return TRUE;
}
