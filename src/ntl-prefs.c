#include "config.h"

#include "nautilus.h"

typedef struct {
  char *label;
  gpointer (*create_prefs_page)(GtkWidget *container);
  void (*pull)(gpointer data);
  void (*apply)(gpointer data);
  gpointer data;
} PrefsPageInfo;

static gpointer window_prefs_create(GtkWidget *container);
static void window_prefs_pull(gpointer data);
static void window_prefs_apply(gpointer data);

PrefsPageInfo pages[] = {
  {N_("Windows"), window_prefs_create, window_prefs_pull, window_prefs_apply, NULL},
  {NULL, NULL, NULL, NULL, NULL}
};

NautilusPrefs nautilus_prefs = {0,0};

void
nautilus_prefs_save(void)
{
  gnome_config_set_bool("/nautilus/prefs/window_alwaysnew", nautilus_prefs.window_alwaysnew);
  gnome_config_set_bool("/nautilus/prefs/window_search_existing", nautilus_prefs.window_search_existing);
}

void
nautilus_prefs_load(void)
{
  nautilus_prefs.window_alwaysnew = 
    gnome_config_get_bool("/nautilus/prefs/window_alwaysnew=0");
  nautilus_prefs.window_search_existing =
    gnome_config_get_bool("/nautilus/prefs/window_search_existing=0");
}

static void
nautilus_prefs_clicked(GnomeDialog *dlg, gint button_number)
{
  int i;

  if(button_number != 0)
    return; /* Ignore it */

  for(i = 0; pages[i].label; i++)
    pages[i].apply(pages[i].data);
}

void
nautilus_prefs_ui_show(GtkWindow *transient_for)
{
  static GtkWidget *dialog = NULL;
  int i;

  if (!dialog)
    {
      GtkWidget *notebook;

      dialog = gnome_dialog_new(_("Preferences"), _("OK"), _("Cancel"), NULL);

      gnome_dialog_close_hides(GNOME_DIALOG(dialog), TRUE);

      notebook = gtk_notebook_new();
      gtk_container_add(GTK_CONTAINER(GNOME_DIALOG(dialog)->vbox), notebook);
      for(i = 0; pages[i].label; i++)
	{
	  GtkWidget *tab_label, *menu_label, *child;

	  tab_label = gtk_label_new(_(pages[i].label));
	  menu_label = gtk_label_new(_(pages[i].label));

	  child = gtk_vbox_new(FALSE, GNOME_PAD);

	  pages[i].data = pages[i].create_prefs_page(child);

	  gtk_notebook_append_page_menu(GTK_NOTEBOOK(notebook), child, tab_label, menu_label);
	}

      gtk_signal_connect(GTK_OBJECT(dialog), "clicked", nautilus_prefs_clicked, NULL);

      gnome_dialog_set_close(GNOME_DIALOG(dialog), TRUE);
      gtk_widget_show_all(dialog);
    }

  for(i = 0; pages[i].label; i++)
    pages[i].pull(pages[i].data);

  gtk_window_set_transient_for(GTK_WINDOW(dialog), transient_for);
  gtk_widget_show(dialog);
}

/* Window prefs page */
typedef struct {
  GtkWidget *check_newwin, *check_search_existing;
} WindowsPrefsInfo;

static gpointer
window_prefs_create(GtkWidget *container)
{
  GtkWidget *table;
  WindowsPrefsInfo *retval;

  retval = g_new0(WindowsPrefsInfo, 1);
  table = gtk_table_new(2, 2, FALSE);

  retval->check_newwin =
    gtk_check_button_new_with_label(_("Create new window for each new page"));

  retval->check_search_existing =
    gtk_check_button_new_with_label(_("Do not open more than one window with the same page"));  

  gtk_table_attach_defaults(GTK_TABLE(table), retval->check_newwin, 0, 2, 0, 1);
  gtk_table_attach_defaults(GTK_TABLE(table), retval->check_search_existing, 0, 2, 1, 2);
  gtk_container_add(GTK_CONTAINER(container), table);

  return retval;
}

static void
window_prefs_pull(gpointer data)
{
  WindowsPrefsInfo *wpi = data;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wpi->check_newwin),
			       nautilus_prefs.window_alwaysnew);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wpi->check_search_existing),
			       nautilus_prefs.window_search_existing);
}

static void
window_prefs_apply(gpointer data)
{
  WindowsPrefsInfo *wpi = data;

  nautilus_prefs.window_alwaysnew = 
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wpi->check_newwin));
  nautilus_prefs.window_search_existing = 
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wpi->check_search_existing));
}
