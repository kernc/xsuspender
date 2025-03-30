#include "xsuspender.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <glib.h>
#include <libwnck/libwnck.h>

#include "config.h"
#include "entry.h"
#include "events.h"
#include "exec.h"
#include "macros.h"
#include "rule.h"


static
GMainLoop *loop;

WnckHandle *handle;

gboolean is_battery_powered;

GSList *suspended_entries;
GSList *queued_entries;

Rule **rules;


gboolean
xsus_signal_stop (WindowEntry *entry)
{
    Rule *rule = entry->rule;

    // Run the windows' designated exec_suspend script, if any.
    // If the subprocess fails, exit early without stopping the process.
    if (xsus_exec_subprocess (rule->exec_suspend, entry) != 0) {
        g_debug ("Subprocess failed; not stopping the process.");
        return FALSE;
    }

    // Mark the process as suspended and kill it
    g_debug ("kill -STOP %d", entry->pid);
    suspended_entries = g_slist_prepend (suspended_entries, entry);

    if (rule->send_signals) {
        kill (entry->pid, SIGSTOP);
        xsus_kill_subtree (entry->pid, SIGSTOP, rule->subtree_pattern);
    }

    return TRUE;
}


gboolean
xsus_signal_continue (WindowEntry *entry)
{
    Rule *rule = entry->rule;

    // Run the window's designated exec_resume script, if any.
    // If the subprocess fails, continue the process anyway. Cause what were
    // you going to do with a stuck process???
    if (xsus_exec_subprocess (rule->exec_resume, entry) != 0)
        g_debug ("Subprocess failed; resuming the process anyway.");

    // Mark the process as not suspended and kill it
    g_debug ("kill -CONT %d", entry->pid);
    suspended_entries = g_slist_remove (suspended_entries, entry);

    if (rule->send_signals) {
        // Resume subprocesses before parent process to avoid the parent
        // workers manager considering them "stuck" (cf. Firefox)
        xsus_kill_subtree (entry->pid, SIGCONT, rule->subtree_pattern);

        kill (entry->pid, SIGCONT);
    }

    // Free the entry now
    xsus_window_entry_free (entry);

    return TRUE;
}


void
xsus_window_entry_enqueue (WindowEntry *entry,
                           unsigned delay) {
    // Mark the time of suspension
    entry->suspend_timestamp = time (NULL) + delay;

    // Schedule suspension
    queued_entries = g_slist_prepend (queued_entries, entry);
}


void
xsus_window_resume (WnckWindow *window)
{
    Rule *rule = xsus_window_get_rule (window);

    // No matching configuration rule, window was not suspended
    if (! rule)
        return;

    WindowEntry *entry;

    // Remove the process from the pending queue
    if ((entry = xsus_entry_find_for_window_rule (window, rule, queued_entries))) {
        g_debug ("Removing window %#lx (%d) from suspension queue: %s",
                 wnck_window_get_xid (window),
                 wnck_window_get_pid (window),
                 wnck_window_get_name (window));
        queued_entries = g_slist_remove (queued_entries, entry);
        xsus_window_entry_free (entry);
        return;
    }

    // Continue the process if it was actually stopped
    if ((entry = xsus_entry_find_for_window_rule (window, rule, suspended_entries))) {
        g_debug ("Resuming window %#lx (%d): %s",
                 wnck_window_get_xid (window),
                 wnck_window_get_pid (window),
                 wnck_window_get_name (window));
        xsus_signal_continue (entry);
        return;
    }
}


void
xsus_window_suspend (WnckWindow *window)
{
    Rule *rule = xsus_window_get_rule (window);

    // No matching configuration rule, nothing to suspend
    if (! rule)
        return;

    // Rule only applies on battery power and we are not
    if (! is_battery_powered && rule->only_on_battery)
        return;

    // We shouldn't be having an entry for this window in the queues already ...
#ifndef NDEBUG
    g_assert (! xsus_entry_find_for_window_rule (window, rule, suspended_entries));
    g_assert (! xsus_entry_find_for_window_rule (window, rule, queued_entries));
#endif

    // Schedule window suspension
    g_debug ("Suspending window in %ds: %#lx (%d): %s",
             rule->delay,
             wnck_window_get_xid (window),
             wnck_window_get_pid (window),
             wnck_window_get_name (window));
    WindowEntry *entry = xsus_window_entry_new (window, rule);
    xsus_window_entry_enqueue (entry, rule->delay);
}


int
xsus_init ()
{
    g_debug ("Initializing.");

    handle = wnck_handle_new (WNCK_CLIENT_TYPE_PAGER);

    // Nowadays common to have a single screen which combines several physical
    // monitors. So it's ok to take the default. See:
    // https://developer.gnome.org/libwnck/stable/WnckScreen.html#WnckScreen.description
    // https://developer.gnome.org/gdk4/stable/GdkScreen.html#GdkScreen.description
    if (! wnck_handle_get_default_screen (handle))
        g_critical ("Default screen is NULL. Not an X11 system? Too bad.");

    // Parse the configuration files
    rules = parse_config ();

    is_battery_powered = FALSE;

    // Init entry lists
    suspended_entries = NULL;
    queued_entries = NULL;

    xsus_init_event_handlers ();

    // Install exit signal handlers to exit gracefully
    signal (SIGINT,  xsus_exit);
    signal (SIGTERM, xsus_exit);
    signal (SIGABRT, xsus_exit);

    // Don't call this function again, we're done
    return FALSE;
}


void
xsus_exit ()
{
    // Quit the main loop and thus, hopefully, exit
    g_debug ("Exiting ...");
    g_main_loop_quit (loop);
}


static inline
void
cleanup ()
{
    wnck_shutdown ();

    // Resume processes we have suspended; deallocate window entries
    GSList *l = suspended_entries;
    while (l) {
        WindowEntry *entry = l->data;
        l = l->next;
        xsus_signal_continue (entry);
    }

    for (GSList *e = queued_entries; e; e = e->next)
        xsus_window_entry_free (e->data);

    g_slist_free (suspended_entries);
    g_slist_free (queued_entries);

    // Resume downclocked processes
    xsus_exit_event_handlers ();

    // Delete rules
    for (int i = 0; rules[i]; ++i)
        xsus_rule_free (rules[i]);
    g_free (rules);
}


static
void
parse_args (int *argc,
            char **argv[])
{
    g_autoptr (GOptionContext) context = g_option_context_new (NULL);

    g_option_context_set_help_enabled (context, FALSE);

    g_option_context_set_summary (context,
        "Automatically suspend inactive (unfocused) X11 windows (processes)\n"
        "to save battery life. (v" PROJECT_VERSION ")");
    g_option_context_set_description (context,
        "The program looks for configuration in ~/.config/xsuspender.conf.\n"
        "Example provided in " EXAMPLE_CONF ".\n"
        "You can copy it over and adapt it to taste.\n"
        "\n"
        "To debug new configuration before it is put into use (recommended),\n"
        "set environment variable G_MESSAGES_DEBUG=xsuspender, i.e.:\n"
        "\n"
        "    G_MESSAGES_DEBUG=xsuspender xsuspender\n"
        "\n"
        "To daemonize the program, run it as:\n"
        "\n"
        "    nohup xsuspender >/dev/null & disown\n"
        "\n"
        "or, better yet, ask your X session manager to run it for you.\n"
        "\n"
        "Read xsuspender(1) manual for more information.");

    if (! g_option_context_parse (context, argc, argv, NULL)) {
        printf ("%s", g_option_context_get_help (context, TRUE, NULL));
        exit (EXIT_FAILURE);
    }
}


int
main (int argc,
      char *argv[])
{
    // Parse command line arguments
    gdk_init (&argc, &argv);
    parse_args (&argc, &argv);

    // Make g_critical() always exit
    g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);

    // Delay initialization until we're within the loop
    g_timeout_add (1, xsus_init, NULL);

    // Enter the main loop
    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    g_main_loop_unref (loop);
    cleanup ();
    g_debug ("Bye.");

    return EXIT_SUCCESS;
}
