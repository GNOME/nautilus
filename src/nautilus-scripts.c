/*
 * Copyright (C) 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-scripts.h"

#include "nautilus-directory.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-program-choosing.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#define MAX_MENU_LEVELS 5
#define TEMPLATE_LIMIT 30
#define SHORTCUTS_PATH "/nautilus/scripts-accels"

struct _NautilusScripts
{
    GObject parent_instance;

    char *scripts_directory_uri;
    int scripts_directory_uri_length;

    GHashTable *script_accels;
    GList *scripts_directory_list;
};

G_DEFINE_FINAL_TYPE (NautilusScripts, nautilus_scripts, G_TYPE_OBJECT)

enum
{
    SCRIPTS_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
scripts_directory_changed (NautilusDirectory *directory,
                           GList             *files,
                           gpointer           user_data);

/* Expected format: accel script_name */
static void
load_custom_accel_for_scripts (NautilusScripts *self)
{
    gchar *path, *contents;
    gchar **lines;
    GError *error = NULL;
    const int max_len = 100;

    path = g_build_filename (g_get_user_config_dir (), SHORTCUTS_PATH, NULL);

    if (g_file_get_contents (path, &contents, NULL, &error))
    {
        lines = g_strsplit (contents, "\n", -1);
        for (guint i = 0; lines[i] != NULL; i++)
        {
            g_auto (GStrv) result = g_strsplit (lines[i], " ", 2);

            if (result[0] == NULL || result[1] == NULL)
            {
                continue;
            }

            g_hash_table_insert (self->script_accels,
                                 g_strndup (result[1], max_len),
                                 g_strndup (result[0], max_len));
        }

        g_free (contents);
        g_strfreev (lines);
    }
    else
    {
        g_debug ("Unable to open '%s', error message: %s", path, error->message);
        g_clear_error (&error);
    }

    g_free (path);
}

static void
add_directory_to_scripts_directory_list (NautilusScripts   *self,
                                         NautilusDirectory *directory)
{
    NautilusAttributes attributes;

    if (g_list_find (self->scripts_directory_list, directory) == NULL)
    {
        nautilus_directory_ref (directory);

        attributes =
            NAUTILUS_ATTRIBUTE_INFO |
            NAUTILUS_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

        nautilus_directory_file_monitor_add (directory, &self->scripts_directory_list,
                                             FALSE, attributes,
                                             (NautilusDirectoryCallback) scripts_directory_changed, self);

        g_signal_connect_object (directory, "files-added",
                                 G_CALLBACK (scripts_directory_changed), self, G_CONNECT_DEFAULT);
        g_signal_connect_object (directory, "files-changed",
                                 G_CALLBACK (scripts_directory_changed), self, G_CONNECT_DEFAULT);

        self->scripts_directory_list = g_list_append (self->scripts_directory_list, directory);
    }
}

static void
remove_directory_from_scripts_directory_list (NautilusScripts   *self,
                                              NautilusDirectory *directory)
{
    self->scripts_directory_list = g_list_remove (self->scripts_directory_list, directory);

    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (scripts_directory_changed),
                                          self);

    nautilus_directory_file_monitor_remove (directory, &self->scripts_directory_list);

    nautilus_directory_unref (directory);
}

static gboolean
directory_belongs_in_scripts_menu (NautilusScripts *self,
                                   const char      *uri)
{
    int num_levels;
    int i;

    if (self->scripts_directory_uri == NULL)
    {
        return FALSE;
    }

    if (!g_str_has_prefix (uri, self->scripts_directory_uri))
    {
        return FALSE;
    }

    num_levels = 0;
    for (i = self->scripts_directory_uri_length; uri[i] != '\0'; i++)
    {
        if (uri[i] == '/')
        {
            num_levels++;
        }
    }

    if (num_levels > MAX_MENU_LEVELS)
    {
        return FALSE;
    }

    return TRUE;
}

static void
scripts_directory_changed (NautilusDirectory *directory,
                           GList             *files,
                           gpointer           user_data)
{
    NautilusScripts *self = NAUTILUS_SCRIPTS (user_data);
    g_autolist (NautilusDirectory) copy = nautilus_directory_list_copy (self->scripts_directory_list);

    for (GList *dir_l = copy; dir_l != NULL; dir_l = dir_l->next)
    {
        g_autofree char *uri = nautilus_directory_get_uri (dir_l->data);

        if (!directory_belongs_in_scripts_menu (self, uri))
        {
            remove_directory_from_scripts_directory_list (self, dir_l->data);
        }
    }

    g_signal_emit (self, signals[SCRIPTS_CHANGED], 0);
}

static gboolean
filter_hidden_scripts (NautilusFile *file,
                       gpointer      callback_data)
{
    return nautilus_file_should_show (file, FALSE);
}

static GMenu *
get_menu_for_view (NautilusScripts   *self,
                   NautilusDirectory *directory,
                   NautilusFilesView *view,
                   AddScriptClosure   closure)
{
    GMenu *menu, *children_menu;
    gboolean any_scripts;
    NautilusFile *file;
    NautilusDirectory *dir;
    char *uri;
    int num;

    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    g_autolist (NautilusFile) file_list = nautilus_directory_get_file_list (directory);

    file_list = nautilus_file_list_filter (file_list, filter_hidden_scripts, NULL);
    file_list = nautilus_file_list_sort_by_display_name (file_list);

    menu = g_menu_new ();

    num = 0;
    any_scripts = FALSE;
    for (NautilusFileList *node = file_list;
         num < TEMPLATE_LIMIT && node != NULL;
         node = node->next, num++)
    {
        file = node->data;
        if (nautilus_file_is_directory (file))
        {
            uri = nautilus_file_get_uri (file);
            if (directory_belongs_in_scripts_menu (self, uri))
            {
                dir = nautilus_directory_get_by_uri (uri);
                add_directory_to_scripts_directory_list (self, dir);

                children_menu = get_menu_for_view (self, dir, view, closure);

                if (children_menu != NULL)
                {
                    const char *file_name = nautilus_file_get_display_name (file);

                    g_menu_append_submenu (menu, file_name, G_MENU_MODEL (children_menu));
                    any_scripts = TRUE;
                    g_object_unref (children_menu);
                }

                nautilus_directory_unref (dir);
            }
            g_free (uri);
        }
        else if (nautilus_file_is_launchable (file))
        {
            (closure) (view, file, menu, self->script_accels);
            any_scripts = TRUE;
        }
    }

    if (!any_scripts)
    {
        g_object_unref (menu);
        menu = NULL;
    }

    return menu;
}

GMenu *
nautilus_scripts_get_menu_for_view (NautilusScripts   *self,
                                    NautilusFilesView *view,
                                    AddScriptClosure   closure)
{
    g_return_val_if_fail (NAUTILUS_IS_SCRIPTS (self), NULL);
    g_return_val_if_fail (NAUTILUS_IS_FILES_VIEW (view), NULL);

    if (self->scripts_directory_uri == NULL)
    {
        return NULL;
    }

    g_autoptr (NautilusDirectory) directory = nautilus_directory_get_by_uri (self->scripts_directory_uri);

    return get_menu_for_view (self, directory, view, closure);
}

static void
nautilus_scripts_dispose (GObject *object)
{
    NautilusScripts *self = NAUTILUS_SCRIPTS (object);

    /* Remove all scripts directory monitors */
    while (self->scripts_directory_list != NULL)
    {
        NautilusDirectory *directory = self->scripts_directory_list->data;

        remove_directory_from_scripts_directory_list (self, directory);
    }

    g_clear_pointer (&self->script_accels, g_hash_table_unref);
    g_clear_pointer (&self->scripts_directory_uri, g_free);

    G_OBJECT_CLASS (nautilus_scripts_parent_class)->dispose (object);
}

static void
nautilus_scripts_init (NautilusScripts *self)
{
    g_autofree gchar *scripts_directory_path = NULL;

    scripts_directory_path = nautilus_get_scripts_directory_path ();

    if (g_mkdir_with_parents (scripts_directory_path, 0700) == 0)
    {
        g_autoptr (GFile) scripts_directory_file = g_file_new_for_path (scripts_directory_path);

        self->scripts_directory_uri = g_file_get_uri (scripts_directory_file);
        self->scripts_directory_uri_length = strlen (self->scripts_directory_uri);
    }

    self->script_accels = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, g_free);
    load_custom_accel_for_scripts (self);

    if (self->scripts_directory_uri != NULL)
    {
        g_autoptr (NautilusDirectory) scripts_directory = nautilus_directory_get_by_uri (self->scripts_directory_uri);

        add_directory_to_scripts_directory_list (self, scripts_directory);
    }
    else
    {
        g_warning ("Ignoring scripts directory, it may be a broken link\n");
    }
}

static void
nautilus_scripts_class_init (NautilusScriptsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_scripts_dispose;

    signals[SCRIPTS_CHANGED] =
        g_signal_new ("scripts-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static NautilusScripts *
nautilus_scripts_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SCRIPTS, NULL);
}

NautilusScripts *
nautilus_scripts_get (void)
{
    static NautilusScripts *singleton = NULL;

    if (G_UNLIKELY (singleton == NULL))
    {
        singleton = nautilus_scripts_new ();
    }

    return singleton;
}
