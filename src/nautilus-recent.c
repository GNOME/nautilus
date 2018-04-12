/*
 * Copyright (C) 2002 James Willcox
 * Copyright (C) 2018 Canonical Ltd
 * Copyright (C) 2018 Marco Trevisan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "nautilus-recent.h"
#include "nautilus-file-private.h"

#include <eel/eel-vfs-extensions.h>

#define DEFAULT_APP_EXEC "gnome-open %u"

static GtkRecentManager *
nautilus_recent_get_manager (void)
{
    static GtkRecentManager *manager = NULL;

    if (manager == NULL)
    {
        manager = gtk_recent_manager_get_default ();
    }

    return manager;
}

void
nautilus_recent_add_file (NautilusFile *file,
                          GAppInfo     *application)
{
    GtkRecentData recent_data;
    char *uri;

    uri = nautilus_file_get_activation_uri (file);
    if (uri == NULL)
    {
        uri = nautilus_file_get_uri (file);
    }

    /* do not add trash:// etc */
    if (eel_uri_is_trash (uri) ||
        eel_uri_is_search (uri) ||
        eel_uri_is_recent (uri))
    {
        g_free (uri);
        return;
    }

    recent_data.display_name = NULL;
    recent_data.description = NULL;

    recent_data.mime_type = nautilus_file_get_mime_type (file);
    recent_data.app_name = g_strdup (g_get_application_name ());

    if (application != NULL)
    {
        recent_data.app_exec = g_strdup (g_app_info_get_commandline (application));
    }
    else
    {
        recent_data.app_exec = g_strdup (DEFAULT_APP_EXEC);
    }

    recent_data.groups = NULL;
    recent_data.is_private = FALSE;

    gtk_recent_manager_add_full (nautilus_recent_get_manager (),
                                 uri, &recent_data);

    g_free (recent_data.mime_type);
    g_free (recent_data.app_name);
    g_free (recent_data.app_exec);

    g_free (uri);
}

void
nautilus_recent_update_file_moved (const gchar *old_uri,
                                   const gchar *new_uri,
                                   const gchar *old_display_name,
                                   const gchar *new_display_name)
{
    GtkRecentManager *recent_manager = nautilus_recent_get_manager ();
    g_autoptr (NautilusFile) file = NULL;
    NautilusFile *existing_file;

    if (new_uri == NULL || old_uri == NULL)
    {
        return;
    }

    existing_file = nautilus_file_get_existing_by_uri (new_uri);

    if (existing_file)
    {
        file = g_object_ref (existing_file);
    }
    else
    {
        g_autoptr (GFile) location = g_file_new_for_uri (new_uri);
        g_autoptr (GFileInfo) file_info = NULL;

        file_info = g_file_query_info (location,
                                       NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                       0, NULL, NULL);
        if (file_info == NULL)
        {
            return;
        }

        file = nautilus_file_get (location);
        nautilus_file_update_info (file, file_info);
    }

    if (nautilus_file_is_directory (file))
    {
        g_autoptr (GFile) location = NULL;
        GList *recent_items;
        GList *l;

        location = g_file_new_for_uri (old_uri);
        recent_items = gtk_recent_manager_get_items (recent_manager);

        for (l = recent_items; l; l = l->next)
        {
            GtkRecentInfo *info = l->data;
            const gchar *item_uri = gtk_recent_info_get_uri (info);

            if (g_str_has_prefix (item_uri, old_uri))
            {
                g_autoptr (GFile) item_file = NULL;
                g_autofree gchar *relative_path = NULL;
                g_autofree gchar *new_item_uri = NULL;

                item_file = g_file_new_for_uri (item_uri);
                relative_path = g_file_get_relative_path (location, item_file);
                new_item_uri = g_build_filename (new_uri, relative_path, NULL);

                gtk_recent_manager_move_item (recent_manager,
                                              item_uri, new_item_uri, NULL);
            }
        }

        g_list_free_full (recent_items, (GDestroyNotify) gtk_recent_info_unref);
    }
    else
    {
        GtkRecentInfo *old_recent;

        old_recent = gtk_recent_manager_lookup_item (recent_manager,
                                                     old_uri, NULL);
        if (old_recent)
        {
            gboolean recreated = FALSE;

            if ((new_display_name != NULL && old_display_name == NULL) ||
                (g_strcmp0 (old_display_name, new_display_name) != 0 &&
                 g_strcmp0 (old_display_name,
                            gtk_recent_info_get_display_name (old_recent)) == 0))
            {
                /* If old display name, matches the one in the recent file
                 * We can't just move it, but we need to recreate the
                 * GtkRecentInfo
                 */
                GtkRecentData recent_data =
                {
                    .display_name = (gchar *) new_display_name,
                    .description = (gchar *) gtk_recent_info_get_description (old_recent),
                    .mime_type = (gchar *) gtk_recent_info_get_mime_type (old_recent),
                    .app_name = gtk_recent_info_last_application (old_recent),
                    .groups = gtk_recent_info_get_groups (old_recent, NULL),
                    .is_private = gtk_recent_info_get_private_hint (old_recent),
                };

                if (recent_data.app_name)
                {
                    gtk_recent_info_get_application_info (old_recent,
                                                          recent_data.app_name,
                                                          (const gchar **) &(recent_data.app_exec),
                                                          NULL, NULL);
                }

                if (gtk_recent_manager_add_full (recent_manager,
                                                 new_uri,
                                                 &recent_data))
                {
                    recreated = gtk_recent_manager_remove_item (recent_manager,
                                                                old_uri,
                                                                NULL);
                }
            }

            if (!recreated)
            {
                gtk_recent_manager_move_item (recent_manager,
                                              old_uri, new_uri, NULL);
            }
        }
        else
        {
            gtk_recent_manager_remove_item (recent_manager, old_uri, NULL);
        }
    }
}
