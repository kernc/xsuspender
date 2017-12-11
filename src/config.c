#include "config.h"

#include <stdlib.h>

#include <glib.h>

#include "rule.h"
#include "macros.h"


static
gboolean error_encountered = FALSE;


static
gboolean
is_error (GError **err)
{
    if (g_error_matches (*err, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_INVALID_VALUE)) {
        g_warning ("Error parsing config: %s", (*err)->message);
        error_encountered = TRUE;
    }
    if (*err) {
        g_error_free (*err);
        *err = NULL;
        return TRUE;
    }
    return FALSE;
}


static
char**
parse_command (GKeyFile *file,
               char *section,
               char *key,
               GError **err)
{
    g_autofree char *value = g_key_file_get_value (file, section, key, err);
    if (! value)
        return NULL;

    char **argv = NULL;
    // Empty command is same as no command
    value = g_strstrip (value);
    if (g_strcmp0 (value, "") != 0) {
        // Pass everything to /bin/sh as this is the most convenient for scripting
        char *args[4] = {"sh", "-c", value, NULL};
        argv = g_strdupv (args);
    }
    return argv;
}


static
void
reassign_strv (char ***existing,
               char **replacement)
{
    // This can be simple because the rest is done in parse_command()
    g_strfreev (*existing);
    *existing = replacement;
}


static
void
reassign_str (char **existing,
              char *replacement)
{
    // If the key was missing, GError should catch it.
    // Perhaps an empty one, but replacement is definitely a string.
    g_assert (replacement != NULL);

    // Free the previous value
    g_free (*existing);
    *existing = NULL;

    // If replacement is a non-empty string, use it
    if (g_strcmp0 (replacement, "") != 0)
        *existing = replacement;
    else
        // Otherwise, we have no use for it
        g_free (replacement);
}


// Remove once glib-2.44+ is ubiquitous
#if GLIB_MINOR_VERSION < 44
static inline
gboolean
g_strv_contains (const gchar *const *strv,
                 const gchar *str)
{
    for (; *strv; strv++)
        if (g_strcmp0 (str, *strv) == 0)
            return TRUE;
    return FALSE;
}
#endif


static
void
read_section (GKeyFile *file,
              char *section,
              Rule *rule)
{
    g_autoptr (GError) err = NULL;

    int val;

    val = g_key_file_get_boolean (file, section, CONFIG_KEY_ONLY_ON_BATTERY, &err);
    if (! is_error (&err)) rule->only_on_battery = (gboolean) val;

    val = g_key_file_get_boolean (file, section, CONFIG_KEY_AUTO_ON_BATTERY, &err);
    if (! is_error (&err)) rule->auto_on_battery = (gboolean) val;

    val = g_key_file_get_boolean (file, section, CONFIG_KEY_SEND_SIGNALS, &err);
    if (! is_error (&err)) rule->send_signals = (gboolean) val;

    val = g_key_file_get_integer (file, section, CONFIG_KEY_SUSPEND_DELAY, &err);
    if (! is_error (&err)) rule->delay = (guint16) CLAMP (val, 1, G_MAXUINT16);

    val = g_key_file_get_integer (file, section, CONFIG_KEY_RESUME_EVERY, &err);
    if (! is_error (&err)) rule->resume_every = (guint16) (val ? CLAMP (val, 1, G_MAXUINT16) : 0);

    val = g_key_file_get_integer (file, section, CONFIG_KEY_RESUME_FOR, &err);
    if (! is_error (&err)) rule->resume_for = (guint16) CLAMP (val, 1, G_MAXUINT16);

    char *str;

    str = g_key_file_get_value (file, section, CONFIG_KEY_WM_CLASS_CONTAINS, &err);
    if (! is_error (&err)) reassign_str (&rule->needle_wm_class, str);

    str = g_key_file_get_value (file, section, CONFIG_KEY_WM_CLASS_GROUP_CONTAINS, &err);
    if (! is_error (&err)) reassign_str (&rule->needle_wm_class_group, str);

    str = g_key_file_get_value (file, section, CONFIG_KEY_WM_NAME_CONTAINS, &err);
    if (! is_error (&err)) reassign_str (&rule->needle_wm_name, str);

    str = g_key_file_get_value (file, section, CONFIG_KEY_SUBTREE_PATTERN, &err);
    if (! is_error (&err)) reassign_str (&rule->subtree_pattern, str);

    char **argv;

    argv = parse_command (file, section, CONFIG_KEY_EXEC_SUSPEND, &err);
    if (! is_error (&err)) reassign_strv (&rule->exec_suspend, argv);

    argv = parse_command (file, section, CONFIG_KEY_EXEC_RESUME, &err);
    if (! is_error (&err)) reassign_strv (&rule->exec_resume, argv);

    g_assert (err == NULL);  // Already freed

    // Ensure all configuration keys are valid
    static const char* VALID_KEYS[] = CONFIG_VALID_KEYS;
    g_auto (GStrv) keys = g_key_file_get_keys (file, section, NULL, NULL);
    for (int i = 0; keys[i]; ++i) {
        if (! g_strv_contains (VALID_KEYS, keys[i])) {
            g_warning ("Invalid key in section '%s': %s", section, keys[i]);
            error_encountered = TRUE;
        }
    }

    // For non-Default sections, ensure at least one window-matching needle is specified
    if (g_strcmp0 (section, CONFIG_DEFAULT_SECTION) != 0 &&
        ! rule->needle_wm_name  &&
        ! rule->needle_wm_class &&
        ! rule->needle_wm_class_group) {
        g_warning ("Invalid rule '%s' matches all windows", section);
        error_encountered = TRUE;
    }
}


static
void
debug_print_rule (Rule *rule)
{
    g_debug ("\n"
                 "needle_wm_class = %s\n"
                 "needle_wm_class_group = %s\n"
                 "needle_wm_name = %s\n"
                 "delay = %d\n"
                 "resume_every = %d\n"
                 "resume_for = %d\n"
                 "only_on_battery = %d\n"
                 "send_signals = %d\n"
                 "subtree_pattern = %s\n"
                 "exec_suspend = %s\n"
                 "exec_resume = %s\n",
             rule->needle_wm_class,
             rule->needle_wm_class_group,
             rule->needle_wm_name,
             rule->delay,
             rule->resume_every,
             rule->resume_for,
             rule->only_on_battery,
             rule->send_signals,
             rule->subtree_pattern,
             (rule->exec_suspend ? rule->exec_suspend[2] : NULL),
             (rule->exec_resume ? rule->exec_resume[2] : NULL)
    );
}


Rule **
parse_config ()
{
    Rule defaults = {
        .needle_wm_name = NULL,
        .needle_wm_class = NULL,
        .needle_wm_class_group = NULL,

        .exec_suspend = NULL,
        .exec_resume = NULL,

        .subtree_pattern = NULL,

        .delay = 10,
        .resume_every = 50,
        .resume_for = 5,

        .send_signals = TRUE,
        .only_on_battery = TRUE,
        .auto_on_battery = TRUE,
    };

    g_autoptr (GKeyFile) file = g_key_file_new ();
    g_autoptr (GError) err = NULL;

    g_autofree char *path = g_build_path ("/", g_get_user_config_dir (), CONFIG_FILE_NAME, NULL);
    g_key_file_load_from_file (file, path, G_KEY_FILE_NONE, &err);

    if (err)
        g_critical ("Cannot read configuration file '%s': %s", path, err->message);

    // Process Default section
    gboolean has_default_section = g_key_file_has_group (file, CONFIG_DEFAULT_SECTION);
    if (has_default_section)
        read_section (file, CONFIG_DEFAULT_SECTION, &defaults);

    // Read all other sections (rules)
    gsize n_sections = 0;
    g_auto (GStrv) sections = g_key_file_get_groups (file, &n_sections);
    n_sections -= has_default_section;

    if (n_sections <= 0)
        g_critical ("No configuration rules found. Nothing to do. Exiting.");

    Rule **rules = g_malloc0_n (n_sections + 1, sizeof (Rule*)),
         **ptr = rules;

    for (int i = 0; sections[i]; ++i) {
        // Skip Default section; it was already handled above
        if (g_strcmp0 (sections[i], CONFIG_DEFAULT_SECTION) == 0)
            continue;

        Rule *rule = xsus_rule_copy (&defaults);
        read_section (file, sections[i], rule);
        *ptr++ = rule;
    }

    // Debug dump rules in effect
    for (int i = 0; rules[i]; ++i)
        debug_print_rule (rules[i]);

    if (error_encountered)
        g_critical ("Errors encountered while parsing config. Abort.");

    return rules;
}
