/*
 * Copyright (C) 2025 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "nautilus-user-dirs-check.h"

#include "nautilus-bookmark-list.h"
#include "nautilus-global-preferences.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <adwaita.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>

typedef struct
{
    char *type;
    char *path;
} XdgDirEntry;

static char *
parse_xdg_dirs_locale (void)
{
    char *file, *content;
    char *locale;

    locale = NULL;
    file = g_build_filename (g_get_user_config_dir (),
                             "user-dirs.locale", NULL);
    if (g_file_get_contents (file, &content, NULL, NULL))
    {
        locale = g_strchug (g_strchomp (content));
    }
    g_free (file);
    return locale;
}

static XdgDirEntry *
parse_xdg_dirs (const char *config_file)
{
    GArray *array;
    char *config_file_free = NULL;
    XdgDirEntry dir;
    char *data;
    char **lines;
    char *p, *d;
    int i;
    char *type_start, *type_end;
    char *value, *unescaped;
    gboolean relative;

    array = g_array_new (TRUE, TRUE, sizeof (XdgDirEntry));

    if (config_file == NULL)
    {
        config_file_free = g_build_filename (g_get_user_config_dir (),
                                             "user-dirs.dirs", NULL);
        config_file = (const char *) config_file_free;
    }

    if (!g_file_get_contents (config_file, &data, NULL, NULL))
    {
        goto out;
    }

    lines = g_strsplit (data, "\n", 0);
    g_free (data);
    for (i = 0; lines[i] != NULL; i++)
    {
        p = lines[i];
        while (g_ascii_isspace (*p))
        {
            p++;
        }

        if (*p == '#')
        {
            continue;
        }

        value = strchr (p, '=');
        if (value == NULL)
        {
            continue;
        }
        *value++ = 0;

        g_strchug (g_strchomp (p));
        if (!g_str_has_prefix (p, "XDG_"))
        {
            continue;
        }
        if (!g_str_has_suffix (p, "_DIR"))
        {
            continue;
        }
        type_start = p + 4;
        type_end = p + strlen (p) - 4;

        while (g_ascii_isspace (*value))
        {
            value++;
        }

        if (*value != '"')
        {
            continue;
        }
        value++;

        relative = FALSE;
        if (g_str_has_prefix (value, "$HOME/"))
        {
            relative = TRUE;
            value += 6;
        }
        else if (*value != '/')
        {
            continue;
        }

        d = unescaped = g_malloc (strlen (value) + 1);
        while (*value && *value != '"')
        {
            if ((*value == '\\') && (*(value + 1) != 0))
            {
                value++;
            }
            *d++ = *value++;
        }
        *d = 0;

        *type_end = 0;
        dir.type = g_strdup (type_start);
        if (relative)
        {
            dir.path = g_build_filename (g_get_home_dir (), unescaped, NULL);
            g_free (unescaped);
        }
        else
        {
            dir.path = unescaped;
        }

        g_array_append_val (array, dir);
    }

    g_strfreev (lines);

out:
    g_free (config_file_free);
    return (XdgDirEntry *) g_array_free (array, FALSE);
}

static XdgDirEntry *
find_dir_entry (XdgDirEntry *entries,
                const char  *type)
{
    int i;

    for (i = 0; entries[i].type != NULL; i++)
    {
        if (strcmp (entries[i].type, type) == 0)
        {
            return &entries[i];
        }
    }
    return NULL;
}

static XdgDirEntry *
find_dir_entry_by_path (XdgDirEntry *entries,
                        const char  *path)
{
    int i;

    for (i = 0; entries[i].type != NULL; i++)
    {
        if (strcmp (entries[i].path, path) == 0)
        {
            return &entries[i];
        }
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
    {
        return TRUE;
    }

    str = "Desktop";

    g_autofree char *domain = g_strdup (textdomain (NULL));

    /* Translate string in xdg-user-dirs domain. String stays unchanged if no translation exists. */
    textdomain ("xdg-user-dirs");
    gboolean has_translation = dgettext ("xdg-user-dirs", str) != str;
    textdomain (domain);

    return has_translation;
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
    {
        *dot = 0;
    }
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
            !WIFEXITED (exit_status) ||
            WEXITSTATUS (exit_status) != 0)
        {
            AdwDialog *dialog = adw_alert_dialog_new (_("There was an error updating the folders"), NULL);

            adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("_OK"));
            adw_dialog_present (dialog, NULL);
        }
        else
        {
            /* Change succeeded, remove any leftover empty directories */
            for (i = 0; old_entries[i].type != NULL; i++)
            {
                /* Never remove homedir */
                if (strcmp (old_entries[i].path, g_get_home_dir ()) == 0)
                {
                    continue;
                }

                /* If the old path is used by the new config, don't remove */
                entry = find_dir_entry_by_path (new_entries, old_entries[i].path);
                if (entry)
                {
                    continue;
                }

                /* Remove the dir, will fail if not empty */
                g_rmdir (old_entries[i].path);
            }
        }
    }
    else if (g_strcmp0 (response, "keep") == 0)
    {
        save_locale ();
    }
    else if (g_strcmp0 (response, "never") == 0)
    {
        g_settings_set_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_XDG_USER_DIR_RENAMING,
                                FALSE);
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
update_locale (XdgDirEntry *old_entries)
{
    XdgDirEntry *new_entries;
    EntryData *data;
    int exit_status;
    int fd;
    char *filename;
    char *cmdline;
    int i, j;
    GListStore *list_store;
    char *std_out, *std_err;
    gboolean has_changes;

    fd = g_file_open_tmp ("dirs-XXXXXX", &filename, NULL);
    if (fd == -1)
    {
        return;
    }
    close (fd);

    cmdline = g_strdup_printf ("xdg-user-dirs-update --force --dummy-output %s", filename);
    if (!g_spawn_command_line_sync (cmdline, &std_out, &std_err, &exit_status, NULL))
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
    if (!WIFEXITED (exit_status) || WEXITSTATUS (exit_status) != 0)
    {
        return;
    }

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
            {
                break;
            }
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
            {
                break;
            }
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

    g_autoptr (GtkBuilder) builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-user-dirs-dialog.ui");
    AdwDialog *dialog = (AdwDialog *) gtk_builder_get_object (builder, "user_dirs_dialog");
    GtkListItemFactory *old_factory = (GtkListItemFactory *) gtk_builder_get_object (builder, "old_factory");
    GtkListItemFactory *new_factory = (GtkListItemFactory *) gtk_builder_get_object (builder, "new_factory");
    GtkNoSelection *selection_model = (GtkNoSelection *) gtk_builder_get_object (builder, "selection_model");

    g_signal_connect (old_factory, "setup", G_CALLBACK (setup), NULL);
    g_signal_connect (old_factory, "bind", G_CALLBACK (bind), GINT_TO_POINTER (0));
    g_signal_connect (new_factory, "setup", G_CALLBACK (setup), NULL);
    g_signal_connect (new_factory, "bind", G_CALLBACK (bind), GINT_TO_POINTER (1));

    gtk_no_selection_set_model (selection_model, G_LIST_MODEL (list_store));

    data = g_new0 (EntryData, 1);
    data->old_entries = old_entries;
    data->new_entries = new_entries;

    g_signal_connect (dialog, "response", G_CALLBACK (on_response), data);
    adw_dialog_present (dialog, NULL);

    while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    {
        g_main_context_iteration (NULL, TRUE);
    }
}

void
nautilus_user_dirs_check_update_locales (NautilusBookmarkList *bookmark_list)
{
    if (!g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_XDG_USER_DIR_RENAMING))
    {
        /* User previously selected to never update directory names */
        return;
    }

    XdgDirEntry *old_entries, *new_entries, *entry;
    XdgDirEntry *desktop_entry;
    GList *bookmarks;
    char *old_locale;
    char *locale, *dot;
    char *uri;

    old_entries = parse_xdg_dirs (NULL);
    old_locale = parse_xdg_dirs_locale ();
    locale = g_strdup (setlocale (LC_MESSAGES, NULL));
    dot = strchr (locale, '.');
    if (dot)
    {
        *dot = 0;
    }

    if (old_locale && *old_locale != 0 &&
        strcmp (old_locale, locale) != 0 &&
        has_xdg_translation ())
    {
        update_locale (old_entries);
    }

    new_entries = parse_xdg_dirs (NULL);

    bookmarks = nautilus_bookmark_list_get_all (bookmark_list);

    if (bookmarks == NULL)
    {
        char *make_bm_for[] =
        {
            "DOCUMENTS",
            "MUSIC",
            "PICTURES",
            "VIDEOS",
            "DOWNLOAD",
            NULL
        };
        /* No previous bookmarks. Generate standard ones */

        desktop_entry = find_dir_entry (new_entries, "DESKTOP");
        for (guint i = 0; make_bm_for[i] != NULL; i++)
        {
            entry = find_dir_entry (new_entries, make_bm_for[i]);

            if (entry && strcmp (entry->path, g_get_home_dir ()) != 0 &&
                (desktop_entry == NULL || strcmp (entry->path, desktop_entry->path) != 0))
            {
                uri = g_filename_to_uri (entry->path, NULL, NULL);
                if (uri)
                {
                    g_autoptr (GFile) file = g_file_new_for_uri (uri);

                    nautilus_bookmark_list_add (bookmark_list, file, -1);
                }
            }
        }
    }
    else
    {
        /* Map old bookmarks that were moved */
        for (guint i = 0; old_entries[i].path != NULL; i++)
        {
            g_autoptr (GFile) location = g_file_new_for_path (old_entries[i].path);

            if (nautilus_bookmark_list_contains (bookmark_list, location))
            {
                g_autoptr (GFile) new_location = g_file_new_for_path (new_entries[i].path);

                nautilus_bookmark_list_change_location (bookmark_list, location, new_location, FALSE);
            }
        }
    }

    g_free (new_entries);
    g_free (old_entries);
}
