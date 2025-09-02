#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include "parse.h"

static XdgDirEntry *
find_dir_entry (XdgDirEntry *entries, const char *type)
{
  int i;

  for (i = 0; entries[i].type != NULL; i++)
    {
      if (strcmp (entries[i].type, type) == 0)
        return &entries[i];
    }
  return NULL;
}

static XdgDirEntry *
find_dir_entry_by_path (XdgDirEntry *entries, const char *path)
{
  int i;

  for (i = 0; entries[i].type != NULL; i++)
    {
      if (strcmp (entries[i].path, path) == 0)
        return &entries[i];
    }
  return NULL;
}

static gboolean
has_xdg_translation (void)
{
  char *str;
  const char *locale;

  locale = setlocale (LC_MESSAGES, NULL);

  if (strncmp (locale, "en_US", 5) == 0 ||
      strcmp (locale, "C") == 0)
    return TRUE;

  str = "Desktop";
  return dgettext ("xdg-user-dirs", str) != str;
}

static void
save_locale (void)
{
  FILE *file;
  char *user_locale_file;
  char *locale, *dot;

  user_locale_file = g_build_filename (g_get_user_config_dir (),
                                       "user-dirs.locale", NULL);
  file = fopen (user_locale_file, "w");
  g_free (user_locale_file);

  if (file == NULL)
    {
      fprintf (stderr, "Can't save user-dirs.locale\n");
      return;
    }

  locale = g_strdup (setlocale (LC_MESSAGES, NULL));
  /* Skip encoding part */
  dot = strchr (locale, '.');
  if (dot)
    *dot = 0;
  fprintf (file, "%s", locale);
  g_free (locale);
  fclose (file);
}

typedef struct
{
  XdgDirEntry *old_entries;
  XdgDirEntry *new_entries;
} EntryData;

static void
on_response (AdwDialog *self,
             gchar     *response,
             EntryData *data)
{
  int exit_status;
  guint i;
  XdgDirEntry *old_entries = data->old_entries;
  XdgDirEntry *new_entries = data->new_entries;
  XdgDirEntry *entry;

  if (g_strcmp0 (response, "update") == 0)
    {
      if (!g_spawn_command_line_sync ("xdg-user-dirs-update --force", NULL, NULL, &exit_status, NULL) ||
          !WIFEXITED(exit_status) ||
          WEXITSTATUS(exit_status) != 0)
        {
          AdwDialog *dialog = adw_alert_dialog_new (_("There was an error updating the folders"), NULL);

          adw_dialog_present (dialog, NULL);
        }
    else
      {
        /* Change succeeded, remove any leftover empty directories */
        for (i = 0; old_entries[i].type != NULL; i++)
          {
            /* Never remove homedir */
            if (strcmp (old_entries[i].path, g_get_home_dir ()) == 0)
              continue;

            /* If the old path is used by the new config, don't remove */
            entry = find_dir_entry_by_path (new_entries, old_entries[i].path);
            if (entry)
              continue;

            /* Remove the dir, will fail if not empty */
            g_rmdir (old_entries[i].path);
          }
      }
    }
  else if (g_strcmp0 (response, "never") == 0)
    {
      save_locale ();
    }

  g_free (new_entries);
  g_free (data);

}

static void
setup (GtkSignalListItemFactory *factory,
       GtkListItem              *list_item,
       gpointer                  user_data)
{
  GtkWidget *label = gtk_label_new ("");
  gtk_list_item_set_child (list_item, label);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
}

static void
bind (GtkSignalListItemFactory *factory,
      GtkListItem              *list_item,
      gpointer                  user_data)
{
  GtkStringList *string_list = GTK_STRING_LIST (gtk_list_item_get_item (list_item));
  guint pos = GPOINTER_TO_INT (user_data);
  const char *label = gtk_string_list_get_string (string_list, pos);
  GtkWidget *child = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (child), label);
}

static void
update_locale (XdgDirEntry    *old_entries)
{
  XdgDirEntry *new_entries;
  EntryData *data;
  AdwDialog *dialog;
  GtkWidget *vbox;
  int exit_status;
  int fd;
  char *filename;
  char *cmdline;
  int i, j;
  GListStore *list_store;
  GtkSelectionModel *selection_model;
  GtkWidget *view;
  GtkColumnViewColumn *column;
  GtkListItemFactory *factory;
  GtkWidget *label;
  char *std_out, *std_err;
  gboolean has_changes;

  fd = g_file_open_tmp ("dirs-XXXXXX", &filename, NULL);
  if (fd == -1)
    return;
  close (fd);
  
  cmdline = g_strdup_printf ("xdg-user-dirs-update --force --dummy-output %s", filename);
  if (!g_spawn_command_line_sync  (cmdline, &std_out, &std_err, &exit_status, NULL))
    {
      g_free (std_out);
      g_free (std_err);
      g_free (cmdline);
      g_unlink (filename);
      g_free (filename);
      return;
    }
  g_free (std_out);
  g_free (std_err);
  g_free (cmdline);
  if (!WIFEXITED(exit_status) || WEXITSTATUS(exit_status) != 0)
    return;

  new_entries = parse_xdg_dirs (filename);
  g_unlink (filename);
  g_free (filename);

  list_store = g_list_store_new (GTK_TYPE_STRING_LIST);
  has_changes = FALSE;
  for (i = 0; old_entries[i].type != NULL; i++)
    {
      for (j = 0; new_entries[j].type != NULL; j++)
        {
          if (strcmp (old_entries[i].type, new_entries[j].type) == 0)
            break;
        }
      if (new_entries[j].type != NULL &&
          strcmp (old_entries[i].path, new_entries[j].path) != 0)
        {
          char *from, *to;
          g_autoptr (GtkStringList) string_list = NULL;
          from = g_filename_display_name (old_entries[i].path);
          to = g_filename_display_name (new_entries[j].path);
          string_list = gtk_string_list_new ((const char *[]){ from, to, NULL });
          g_list_store_append (list_store, string_list);
          
          g_free (from);
          g_free (to);
          
          has_changes = TRUE;
        }
    }
  for (j = 0; new_entries[j].type != NULL; j++)
    {
      for (i = 0; old_entries[i].type != NULL; i++)
        {
          if (strcmp (old_entries[i].type, new_entries[j].type) == 0)
            break;
        }
      if (old_entries[i].type == NULL)
        {
          char *to;
          g_autoptr (GtkStringList) string_list = NULL;
          to = g_filename_display_name (new_entries[j].path);
          string_list = gtk_string_list_new ((const char *[]){ "-", to, NULL });
          g_list_store_append (list_store, string_list);

          g_free (to);
          
          has_changes = TRUE;
        }
    }

  if (!has_changes)
    {
      return;
    }

  dialog = adw_alert_dialog_new (_("Update standard folders to current language?"),
                                 _("You have logged in in a new language. You can automatically update the names of some standard folders in your home folder to match this language. The update would change the following folders:"));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "never", _("Never Update"),
                                  "keep", _("_Keep Old Names"),
                                  "update", _("Update Names"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "update", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_prefer_wide_layout (ADW_ALERT_DIALOG (dialog), TRUE);

  selection_model = GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (list_store)));
  view = gtk_column_view_new (selection_model);
  gtk_widget_add_css_class (view, "frame");
  gtk_column_view_set_show_row_separators (GTK_COLUMN_VIEW (view), TRUE);
  gtk_column_view_set_reorderable (GTK_COLUMN_VIEW (view), FALSE);
  
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind), GINT_TO_POINTER (0));

  column = gtk_column_view_column_new (_("Current folder name"), factory);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (view), column);
  g_object_unref (column);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind), GINT_TO_POINTER (1));

  column = gtk_column_view_column_new (_("New folder name"), factory);
  gtk_column_view_column_set_expand (column, TRUE);
  gtk_column_view_append_column (GTK_COLUMN_VIEW (view), column);
  g_object_unref (column);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  label = gtk_label_new (_("Note that existing content will not be moved."));
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  
  gtk_box_append (GTK_BOX (vbox), view);
  gtk_box_append (GTK_BOX (vbox), label);
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), vbox);

  data = g_new0 (EntryData, 1);
  data->old_entries = old_entries;
  data->new_entries = new_entries;

  g_signal_connect (dialog, "response", G_CALLBACK (on_response), data);
  adw_dialog_present (dialog, NULL);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);
}

int
main (int argc, char *argv[])
{
  XdgDirEntry *old_entries, *new_entries, *entry;
  XdgDirEntry *desktop_entry;
  GtkBookmark *bookmark;
  GList *bookmarks, *l;
  char *old_locale;
  char *locale, *dot;
  int i;
  gboolean modified_bookmarks;
  char *uri;
  
  setlocale (LC_ALL, "");
  
  bindtextdomain (GETTEXT_PACKAGE, GLIBLOCALEDIR);
  bindtextdomain ("xdg-user-dirs", GLIBLOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  old_entries = parse_xdg_dirs (NULL);
  old_locale = parse_xdg_dirs_locale ();
  locale = g_strdup (setlocale (LC_MESSAGES, NULL));
  dot = strchr (locale, '.');
  if (dot)
    *dot = 0;

  if (old_locale && *old_locale != 0 &&
      strcmp (old_locale, locale) != 0 &&
      has_xdg_translation ())
    {
       g_set_prgname ("user-dirs-update-gtk");
      adw_init ();
      update_locale (old_entries);
    }
  
  new_entries = parse_xdg_dirs (NULL);

  bookmarks = parse_gtk_bookmarks ();

  modified_bookmarks = FALSE;
  if (bookmarks == NULL)
    {
      char *make_bm_for[] = {
        "DOCUMENTS",
        "MUSIC",
        "PICTURES",
        "VIDEOS",
        "DOWNLOAD",
        NULL};
      /* No previous bookmarks. Generate standard ones */

      desktop_entry = find_dir_entry (new_entries, "DESKTOP");
      for (i = 0; make_bm_for[i] != NULL; i++)
        {
          entry = find_dir_entry (new_entries, make_bm_for[i]);
          
          if (entry && strcmp (entry->path, g_get_home_dir ()) != 0 &&
              (desktop_entry == NULL || strcmp (entry->path, desktop_entry->path) != 0))
            {
              uri = g_filename_to_uri (entry->path, NULL, NULL);
              if (uri)
                {
                  modified_bookmarks = TRUE;
                  bookmark = g_new0 (GtkBookmark, 1);
                  bookmark->uri = uri;
                  bookmarks = g_list_append (bookmarks, bookmark);
                }
            }
        }
    }
  else
    {
      /* Map old bookmarks that were moved */

      for (l = bookmarks; l != NULL; l = l->next)
        {
          char *path;
          
          bookmark = l->data;

          path = g_filename_from_uri (bookmark->uri, NULL, NULL);
          if (path)
            {
              entry = find_dir_entry_by_path (old_entries, path);
              if (entry)
                {
                  entry = find_dir_entry (new_entries, entry->type);
                  if (entry)
                    {
                      uri = g_filename_to_uri (entry->path, NULL, NULL);
                      if (uri)
                        {
                          modified_bookmarks = TRUE;
                          g_free (bookmark->uri);
                          bookmark->uri = uri;
                        }
                    }
                }
              g_free (path);
            }
        }
    }

  if (modified_bookmarks)
    save_gtk_bookmarks (bookmarks);
  
  g_free (new_entries);
  g_free (old_entries);
  
  return 0;
}
