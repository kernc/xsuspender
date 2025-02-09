#include "entry.h"

#include <glib.h>
#include <libwnck/libwnck.h>

#include "proc.h"

WindowEntry*
xsus_window_entry_new (WnckWindow *window,
                       Rule *rule)
{
    WindowEntry *entry = g_malloc (sizeof (WindowEntry));
    entry->rule = rule;
    entry->pid = xsus_window_get_pid (window);
    entry->xid = wnck_window_get_xid (window);
    entry->wm_name = g_strdup (wnck_window_get_name (window));
    return entry;
}


WindowEntry*
xsus_window_entry_copy (WindowEntry *entry)
{
    WindowEntry *copy = g_memdup2 (entry, sizeof (WindowEntry));
    copy->wm_name = g_strdup (copy->wm_name);
    return copy;
}


void
xsus_window_entry_free (WindowEntry *entry)
{
    if (! entry)
        return;
    g_free (entry->wm_name);
    g_free (entry);
}


inline
WindowEntry*
xsus_entry_find_for_window_rule (WnckWindow *window,
                                 Rule *rule,
                                 GSList *list)
{
    // If suspending by signals, find entry by PID ...
    if (rule->send_signals) {
        pid_t pid = xsus_window_get_pid (window);
        for (; list; list = list->next) {
            WindowEntry *entry = list->data;
            if (entry->pid == pid)
                return entry;
        }
    } else {
        // ... else find it by XID
        XID xid = wnck_window_get_xid (window);
        for (; list; list = list->next) {
            WindowEntry *entry = list->data;
            if (entry->xid == xid)
                return entry;
        }
    }
    return NULL;
}
