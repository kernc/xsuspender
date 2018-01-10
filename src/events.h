#ifndef XSUSPENDER_EVENT_HANDLERS_H
#define XSUSPENDER_EVENT_HANDLERS_H

#include "xsuspender.h"


#define SUSPEND_PENDING_INTERVAL  1
#define PERIODIC_RESUME_INTERVAL  1
#define SLOW_INTERVAL            10
#define FAST_INTERVAL_MSEC      100


void xsus_init_event_handlers ();
void xsus_exit_event_handlers ();

void on_active_window_changed (WnckScreen *screen, WnckWindow *prev_active_window);
int  on_suspend_pending_windows ();
int  on_periodic_window_wake_up ();
int  on_check_battery_powered ();
int  on_downclock_slice ();
int  on_update_downclocked_processes ();


#endif  // XSUSPENDER_EVENT_HANDLERS_H
