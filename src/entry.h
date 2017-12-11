#ifndef XSUSPENDER_ENTRY_H
#define XSUSPENDER_ENTRY_H

#include <sys/types.h>

#include <glib.h>
#include <libwnck/libwnck.h>

#include "rule.h"


typedef gulong XID;


// Window entry in the hash tables
typedef struct WindowEntry {
    Rule *rule;
    char *wm_name;  // Window title

    // The X ID and PID of the window that triggered the rule
    XID xid;
    pid_t pid;

    // Timestamp of the next (and last) window suspension by this rule.
    time_t suspend_timestamp;
} WindowEntry;


WindowEntry* xsus_window_entry_new (WnckWindow *window, Rule *rule);
WindowEntry* xsus_window_entry_copy (WindowEntry *entry);
void         xsus_window_entry_free (WindowEntry *entry);

WindowEntry* xsus_entry_find_for_window_rule (WnckWindow *window, Rule *rule, GSList *where);


#endif  // XSUSPENDER_ENTRY_H
