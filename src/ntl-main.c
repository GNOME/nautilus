#include "config.h"
#include "nautilus.h"
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>

static int window_count = 0;
static void
check_for_quit(void)
{
  if(--window_count <= 0)
    gtk_main_quit();
}

int main(int argc, char *argv[])
{
  poptContext ctx;
  CORBA_Environment ev;
  CORBA_ORB orb;
  struct poptOption options[] = {
    {NULL}
  };
  GtkWidget *mainwin;

  orb = gnome_CORBA_init_with_popt_table("nautilus", VERSION, &argc, argv, options, 0, &ctx, GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  mainwin = gtk_widget_new(nautilus_window_get_type(), "app_id", "nautilus", NULL);
  bonobo_activate();
  nautilus_window_set_initial_state(NAUTILUS_WINDOW(mainwin));
  gtk_widget_show(mainwin);
  window_count++;

  gtk_signal_connect(GTK_OBJECT(mainwin), "destroy", check_for_quit, NULL);

  bonobo_main();
  return 0;
}
