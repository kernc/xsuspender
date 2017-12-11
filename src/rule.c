#include "rule.h"

#include <glib.h>
#include <libwnck/libwnck.h>

#include "xsuspender.h"

Rule*
xsus_rule_copy (Rule *orig)
{
    Rule *rule = g_memdup (orig, sizeof (Rule));

    // Duplicate strings explicitly
    rule->needle_wm_name = g_strdup (orig->needle_wm_name);
    rule->needle_wm_class = g_strdup (orig->needle_wm_class);
    rule->needle_wm_class_group = g_strdup (orig->needle_wm_class_group);

    rule->exec_suspend = g_strdupv (orig->exec_suspend);
    rule->exec_resume = g_strdupv (orig->exec_resume);

    return rule;
}


void
xsus_rule_free (Rule *rule)
{
    g_free (rule->needle_wm_class);
    g_free (rule->needle_wm_class_group);
    g_free (rule->needle_wm_name);

    g_strfreev (rule->exec_suspend);
    g_strfreev (rule->exec_resume);

    g_free (rule);
}


static inline
gboolean
str_contains (const char *haystack,
              const char *needle)
{
    if (! needle)
        return TRUE;
    if (! haystack)
        return FALSE;

    return g_strstr_len (haystack, -1, needle) != NULL;
}


Rule*
xsus_window_get_rule (WnckWindow *window)
{
    if (! WNCK_IS_WINDOW (window))
        return NULL;

    const char *wm_name = wnck_window_get_name (window),
              *wm_class = wnck_window_get_class_instance_name (window),
        *wm_class_group = wnck_window_get_class_group_name (window);

    for (int i = 0; rules[i]; ++i) {
        Rule *rule = rules[i];
        // If all provided matching specifiers match
        if (str_contains (wm_class_group, rule->needle_wm_class_group) &&
            str_contains (wm_class, rule->needle_wm_class) &&
            str_contains (wm_name, rule->needle_wm_name)) {
            return rule;
        }
    }
    return NULL;
}
