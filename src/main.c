/*
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>


//kill(pid_t pid, int sig);

#define WNCK_I_KNOW_THIS_IS_UNSTABLE

#include <libwnck/libwnck.h>


static void on_window_opened (WnckScreen *screen, WnckWindow *window, gpointer data)
{

  g_print ("%s\n", wnck_window_get_name (window)); // se ni aktivnih oken
}

static void on_active_window_changed (WnckScreen *screen, WnckWindow *previously_active_window, gpointer data)
{
  WnckWindow *active_window;

  active_window = wnck_screen_get_active_window (screen);
  if (active_window)
    g_print ("active: %s PID: %d \n", wnck_window_get_name (active_window), wnck_window_get_pid(active_window));
  else
    g_print ("no active window\n");
}

int main (int argc,char **argv)
{

  //inicializacija gnome drawing kit, objekta screen in povezanih seznamov (imena oken. PID stevilke, indeksi )
  gdk_init (&argc, &argv);
  WnckScreen *screen = wnck_screen_get_default ();
  wnck_screen_force_update (screen);
  GList *ListOfIndexes, *ListOfNames, *ListOfPIDs, *window_l;
  ListOfIndexes = NULL, ListOfNames = NULL; ListOfPIDs = NULL;


  for (window_l = wnck_screen_get_windows (screen); window_l != NULL; window_l = window_l->next)
    {
      WnckWindow *window = WNCK_WINDOW (window_l->data);

        if(window==wnck_screen_get_active_window (screen)){
            printf("found active window,killing it %s \n",wnck_window_get_name(window));
            kill(wnck_window_get_pid(window), SIGSTOP);
            sleep(5);
            kill(wnck_window_get_pid(window), SIGCONT);


        }

        if (!strcmp(wnck_window_get_name(window),"Facebook - Chromium")){
            printf("ubijam %s \n",wnck_window_get_name(window));


        }

        ListOfPIDs = g_list_append (ListOfPIDs,wnck_window_get_pid((window)) );
        ListOfNames = g_list_append(ListOfNames, wnck_window_get_name(window));

    }

    //printf("Ime:%s PID:%d \n", ListOfNames->next->next->data, ListOfPIDs->data);
    printf("\n\n");


  GMainLoop *loop;
  loop = g_main_loop_new (NULL, FALSE);

  //g_signal_connect (screen, "window-opened", G_CALLBACK (on_window_opened), NULL);
  g_signal_connect (screen, "active-window-changed", G_CALLBACK (on_active_window_changed), NULL);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);


  return 0;
}
*/
































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
