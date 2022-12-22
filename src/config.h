#ifndef XSUSPENDER_PARSE_CONFIG_H
#define XSUSPENDER_PARSE_CONFIG_H

#include "xsuspender.h"
#include "rule.h"

#define CONFIG_FILE_NAME                   (PROJECT_NAME ".conf")

#define CONFIG_DEFAULT_SECTION             "Default"

#define CONFIG_KEY_EXEC_SUSPEND            "exec_suspend"
#define CONFIG_KEY_EXEC_RESUME             "exec_resume"
#define CONFIG_KEY_SUSPEND_DELAY           "suspend_delay"
#define CONFIG_KEY_RESUME_EVERY            "resume_every"
#define CONFIG_KEY_RESUME_FOR              "resume_for"
#define CONFIG_KEY_ONLY_ON_BATTERY         "only_on_battery"
#define CONFIG_KEY_AUTO_ON_BATTERY         "auto_suspend_on_battery"
#define CONFIG_KEY_SEND_SIGNALS            "send_signals"
#define CONFIG_KEY_SUBTREE_PATTERN         "suspend_subtree_pattern"
#define CONFIG_KEY_PROCESS_NAME            "process_name"
#define CONFIG_KEY_DOWNCLOCK_ON_BATTERY    "downclock_on_battery"
#define CONFIG_KEY_WM_NAME_CONTAINS        "match_wm_name_contains"
#define CONFIG_KEY_WM_CLASS_CONTAINS       "match_wm_class_contains"
#define CONFIG_KEY_WM_CLASS_GROUP_CONTAINS "match_wm_class_group_contains"

#define CONFIG_VALID_KEYS  {CONFIG_KEY_ONLY_ON_BATTERY,          \
                            CONFIG_KEY_AUTO_ON_BATTERY,          \
                            CONFIG_KEY_SEND_SIGNALS,             \
                            CONFIG_KEY_SUBTREE_PATTERN,          \
                            CONFIG_KEY_PROCESS_NAME,             \
                            CONFIG_KEY_DOWNCLOCK_ON_BATTERY,     \
                            CONFIG_KEY_SUSPEND_DELAY,            \
                            CONFIG_KEY_RESUME_EVERY,             \
                            CONFIG_KEY_RESUME_FOR,               \
                            CONFIG_KEY_WM_CLASS_CONTAINS,        \
                            CONFIG_KEY_WM_CLASS_GROUP_CONTAINS,  \
                            CONFIG_KEY_WM_NAME_CONTAINS,         \
                            CONFIG_KEY_EXEC_SUSPEND,             \
                            CONFIG_KEY_EXEC_RESUME,              \
                            NULL}


Rule** parse_config ();


#endif  // XSUSPENDER_PARSE_CONFIG_H
