#include "nautilus.h"
#include <file-manager/fm-public-api.h>

static int window_count = 0;

static GnomeObject *
nautilus_make_object(GnomeGenericFactory *gfact, const char *goad_id, gpointer closure)
{
  GtkObject *theobj = NULL;

  if(!strcmp(goad_id, "ntl_file_manager"))
    theobj = gtk_object_new(fm_directory_view_get_type(), NULL);

  if(!theobj)
    return NULL;

  if(GNOME_IS_OBJECT(theobj))
    return GNOME_OBJECT(theobj);

  if(NAUTILUS_IS_VIEW_CLIENT(theobj))
    {
      gtk_widget_show(GTK_WIDGET(theobj));
      return nautilus_view_client_get_gnome_object(NAUTILUS_VIEW_CLIENT(theobj));
    }

  gtk_object_destroy(theobj);

  return NULL;
}

/**
 * nautilus_app_exiting:
 * 
 * Called after the main event loop has finished, just before the
 * program ends. Don't call from anywhere else.
 **/
void
nautilus_app_exiting()
{
  /* Do those things that gotta be done just once before quitting */
  nautilus_prefs_save();
  nautilus_bookmarks_menu_exiting();
}

void
nautilus_app_init(const char *initial_url)
{
  NautilusWindow *mainwin;

  nautilus_navinfo_init();
  nautilus_prefs_load();

  /* Create our CORBA objects */
  gnome_generic_factory_new_multi("nautilus_factory", nautilus_make_object, NULL);

  /* Set default configuration */
  mainwin = nautilus_app_create_window();
  bonobo_activate();
  nautilus_window_set_initial_state(mainwin, initial_url);
}

static void
nautilus_app_destroy_window(GtkObject *obj)
{
  window_count--;

  if(window_count <= 0)
    {
      gtk_main_quit();
    }
}

NautilusWindow *
nautilus_app_create_window(void)
{
  GtkWidget *win = gtk_widget_new(nautilus_window_get_type(), "app_id", "nautilus", NULL);

  window_count++;

  gtk_signal_connect(GTK_OBJECT(win), "destroy", nautilus_app_destroy_window, NULL);

  gtk_widget_show(win);

  return NAUTILUS_WINDOW(win);
}
