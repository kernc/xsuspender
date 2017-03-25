#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

//kill(pid_t pid, int sig);

static void print_active_window (WnckScreen *screen, WnckWindow *previously_active_window, gpointer data);
static void get_window_list (WnckScreen *screen, WnckWindow *previously_active_window, gpointer data);
static void print_window_list (WnckScreen *screen, WnckWindow *previously_active_window, gpointer data);
static void minimize_passive(WnckScreen *screen, WnckWindow *previously_active_window, gpointer data);

GList *ListOfNames, *ListOfPIDs, *ListOfStatus;

int main(int argc, char **argv)
{
    gdk_init (&argc, &argv);

    WnckScreen *screen = wnck_screen_get_default ();

    GMainLoop * loop;
    loop = g_main_loop_new (NULL, FALSE);
    //g_signal_connect (screen, "active-window-changed", G_CALLBACK(print_active_window), NULL);
    g_signal_connect (screen, "active-window-changed", G_CALLBACK(get_window_list), NULL);
    g_signal_connect (screen, "active-window-changed", G_CALLBACK(print_window_list), NULL);
    g_signal_connect (screen, "active-window-changed", G_CALLBACK(minimize_passive), NULL);
    g_main_loop_run (loop);
    g_main_loop_unref(loop);


    return 0;
}


static void print_active_window (WnckScreen *screen, WnckWindow *previously_active_window, gpointer data)
{
    WnckWindow *active_window = wnck_screen_get_active_window (screen);
    if (active_window)
        g_print ("\nactive: %s PID: %d \n", wnck_window_get_name (active_window), wnck_window_get_pid(active_window));
}


static void get_window_list(WnckScreen *screen, WnckWindow *previously_active_window, gpointer data){

    GList *window_list;
        for (window_list = wnck_screen_get_windows (screen); window_list != NULL; window_list = window_list->next){
        WnckWindow *window = WNCK_WINDOW (window_list->data);

        ListOfNames = g_list_append(ListOfNames, wnck_window_get_name(window));
        ListOfPIDs = g_list_append(ListOfPIDs, wnck_window_get_pid(window));

        WnckWindow *active_window = wnck_screen_get_active_window (screen);
        if (active_window==window)
        ListOfStatus = g_list_append(ListOfStatus, "active :");
        else
        ListOfStatus = g_list_append(ListOfStatus, "passive");

    }


}


static void print_window_list(WnckScreen *screen, WnckWindow *previously_active_window, gpointer data){

    GList *window_list;
        for (window_list = wnck_screen_get_windows (screen); window_list != NULL; window_list = window_list->next){
            printf("%d %s %s \n",ListOfPIDs->data,ListOfStatus->data,ListOfNames->data);
            ListOfNames = ListOfNames->next;
            ListOfPIDs = ListOfPIDs->next;
            ListOfStatus = ListOfStatus->next;
        }
        printf("\n");


}


static void minimize_passive(WnckScreen *screen, WnckWindow *previously_active_window, gpointer data){




}
