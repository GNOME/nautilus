#include "config.h"

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifndef g_alloca
#define g_alloca alloca
#endif

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

static gpointer meta_prefs_create(GtkWidget *container);
static void meta_prefs_pull(gpointer data);
static void meta_prefs_apply(gpointer data);

PrefsPageInfo pages[] = {
  {N_("Windows"), window_prefs_create, window_prefs_pull, window_prefs_apply, NULL},
  {N_("Meta Views"), meta_prefs_create, meta_prefs_pull, meta_prefs_apply, NULL},
  {NULL, NULL, NULL, NULL, NULL}
};

NautilusPrefsOld nautilus_prefs = {0,0, NULL};

void
nautilus_prefs_save(void)
{
  char **pieces;
  int npieces, i;
  GSList *cur;

  npieces = g_slist_length(nautilus_prefs.global_meta_views);
  pieces = g_alloca(sizeof(char *) * npieces);
  for(i = 0, cur = nautilus_prefs.global_meta_views; cur; i++, cur = cur->next)
    pieces[i] = cur->data;
  gnome_config_set_vector("/nautilus/prefs/global_meta_views", npieces, (const char **)pieces);

  gnome_config_set_bool("/nautilus/prefs/caca::window_alwaysnew", nautilus_prefs.window_alwaysnew);
  gnome_config_set_bool("/nautilus/prefs/caca2::window_search_existing", nautilus_prefs.window_search_existing);

  gnome_config_sync();
}

void
nautilus_prefs_load(void)
{
  if(nautilus_prefs.global_meta_views)
    {
      g_slist_foreach(nautilus_prefs.global_meta_views, (GFunc)g_free, NULL);
      g_slist_free(nautilus_prefs.global_meta_views);
      nautilus_prefs.global_meta_views = NULL;
    }

  nautilus_prefs.window_alwaysnew = 
    gnome_config_get_bool("/nautilus/prefs/caca::window_alwaysnew=0");
  nautilus_prefs.window_search_existing =
    gnome_config_get_bool("/nautilus/prefs/caca2::window_search_existing=0");

  {
    int npieces;
    char **pieces;
    int i;

    gnome_config_get_vector("/nautilus/prefs/global_meta_views=ntl_history_view ntl_websearch_view ntl_notes_view hyperbola_navigation_search", &npieces, &pieces);

    for(i = 0; i < npieces; i++)
      nautilus_prefs.global_meta_views = g_slist_prepend(nautilus_prefs.global_meta_views, pieces[i]);
    g_free(pieces);
  }
}

static void
nautilus_prefs_clicked(GnomeDialog *dlg, gint button_number)
{
  int i;

  if(button_number != 0)
    return; /* Ignore it */

  for(i = 0; pages[i].label; i++)
    pages[i].apply(pages[i].data);

  nautilus_prefs_save();
}

void
nautilus_prefs_ui_show(GtkWindow *transient_for)
{
  static GtkWidget *dialog = NULL;
  int i;

  if (!dialog)
    {
      GtkWidget *notebook;

      dialog = gnome_dialog_new(_("General Settings"), _("OK"), _("Cancel"), NULL);

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

/* Meta view prefs page */
typedef struct {
  char *iid, *desc;

  GtkWidget *cb_enabled, *litem;
} MetaViewInfo;

typedef struct {
  GtkWidget *list_avail;
  GHashTable *entries;
  GSList *entry_list;
} MetaPrefsInfo;

static gint
mvi_compare(gconstpointer a, gconstpointer b)
{
  const MetaViewInfo *mvia = a, *mvib = b;

  return strcoll(mvia->desc, mvib->desc);
}

static void
meta_prefs_add_entry(MetaPrefsInfo *mpi, const char *iid, const char *desc)
{
  MetaViewInfo *mvi;

  mvi = g_hash_table_lookup(mpi->entries, iid);

  if(mvi)
    return;

  mvi = g_new0(MetaViewInfo, 1);
  mvi->iid = g_strdup(iid);
  mvi->desc = g_strdup(desc);
  mvi->cb_enabled = gtk_check_button_new_with_label(desc);
  mvi->litem = gtk_list_item_new();
  gtk_container_add(GTK_CONTAINER(mvi->litem), mvi->cb_enabled);

  g_hash_table_insert(mpi->entries, mvi->iid, mvi);

  mpi->entry_list = g_slist_insert_sorted(mpi->entry_list, mvi, mvi_compare);
}

static gpointer
meta_prefs_create(GtkWidget *container)
{
  MetaPrefsInfo *mpi;
  GList *items;
  GSList *cur;
  GtkWidget *swin;

  mpi = g_new0(MetaPrefsInfo, 1);
  mpi->entries = g_hash_table_new(g_str_hash, g_str_equal);

  swin = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(container), swin);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  mpi->list_avail = gtk_list_new();
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), mpi->list_avail);

  meta_prefs_add_entry(mpi, "ntl_notes_view", "Annotations");
  meta_prefs_add_entry(mpi, "hyperbola_navigation_tree", "Help Contents");
  meta_prefs_add_entry(mpi, "hyperbola_navigation_index", "Help Index");
  meta_prefs_add_entry(mpi, "hyperbola_navigation_search", "Help Search");
  meta_prefs_add_entry(mpi, "ntl_history_view", "History");
  meta_prefs_add_entry(mpi, "ntl_websearch_view", "Web Search");

  for(items = NULL, cur = mpi->entry_list; cur; cur = cur->next)
    {
      MetaViewInfo *mvi = cur->data;

      items = g_list_append(items, mvi->litem);
    }

  gtk_list_append_items(GTK_LIST(mpi->list_avail), items);

  return mpi;
}

static void
meta_prefs_pull(gpointer data)
{
  MetaPrefsInfo *mpi = data;
  GSList *cur;

  for(cur = mpi->entry_list; cur; cur = cur->next)
    {
      MetaViewInfo *mvi = cur->data;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mvi->cb_enabled), FALSE);
    }
  for(cur = nautilus_prefs.global_meta_views; cur; cur = cur->next)
    {
      MetaViewInfo *mvi = g_hash_table_lookup(mpi->entries, cur->data);

      if(!mvi)
	continue;

      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mvi->cb_enabled), TRUE);
    }
}

static void
meta_prefs_apply(gpointer data)
{
  MetaPrefsInfo *mpi = data;
  GSList *cur, *new_list;

  g_slist_foreach(nautilus_prefs.global_meta_views, (GFunc)g_free, NULL);
  g_slist_free(nautilus_prefs.global_meta_views);

  for(new_list = NULL, cur = mpi->entry_list; cur; cur = cur->next)
    {
      MetaViewInfo *mvi = cur->data;
      if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mvi->cb_enabled)))
	new_list = g_slist_prepend(new_list, g_strdup(mvi->iid));
    }

  nautilus_prefs.global_meta_views = new_list;
}
