#ifndef XSUSPENDER_RULE_H
#define XSUSPENDER_RULE_H

#include <glib.h>
#include <libwnck/libwnck.h>


// Configuration rule
typedef struct Rule {
    char*  needle_wm_name;
    char*  needle_wm_class;
    char*  needle_wm_class_group;

    char** exec_suspend;
    char** exec_resume;

    char*  subtree_pattern;

    char*  process_name;

    guint16 delay;
    guint16 resume_every;
    guint16 resume_for;

    gboolean send_signals;
    gboolean only_on_battery;
    gboolean auto_on_battery;

    guint8 downclock_on_battery;
} Rule;


Rule* xsus_rule_copy (Rule *rule);
void  xsus_rule_free (Rule *rule);
Rule* xsus_window_get_rule (WnckWindow *window);


#endif  // XSUSPENDER_RULE_H
