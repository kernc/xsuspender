#ifndef XSUSPENDER_EVENT_HANDLERS_H
#define XSUSPENDER_EVENT_HANDLERS_H

#include "xsuspender.h"


#define SUSPEND_PENDING_INTERVAL  1
#define PERIODIC_RESUME_INTERVAL  1
#define CHECK_BATTERY_INTERVAL   10


void xsus_init_event_handlers ();

void on_active_window_changed (WnckScreen *screen, WnckWindow *prev_active_window);
int  on_suspend_pending_windows ();
int  on_periodic_window_wake_up ();
int  on_check_battery_powered ();


#endif  // XSUSPENDER_EVENT_HANDLERS_H
