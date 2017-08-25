/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nautilus-tasks.h"

#include "nautilus-file.h"
#include "nautilus-file-changes.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#define GNOME_DESKTOP_USE_UNSTABLE_API
#endif

#include <libgnome-desktop/gnome-desktop-thumbnail.h>

const char *const DEFAULT_ATTRIBUTES = "standard::*,"
                                       "access::*,"
                                       "mountable::*,"
                                       "time::*,"
                                       "unix::*,"
                                       "owner::*,"
                                       "selinux::*,"
                                       "thumbnail::*,"
                                       "id::filesystem,"
                                       "trash::orig-path,"
                                       "trash::deletion-date,"
                                       "metadata::*,"
                                       "recent::*";

void
nautilus_enumerate_children_task_func (NautilusTask *task,
                                       gpointer      data)
{
    GFile *location;
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;
    g_autoptr (GFileEnumerator) enumerator = NULL;
    GHashTable *hash_table;
    GFileInfo *info;

    location = data;
    cancellable = nautilus_task_get_cancellable (task);
    enumerator = g_file_enumerate_children (location, DEFAULT_ATTRIBUTES,
                                            G_FILE_QUERY_INFO_NONE, cancellable, &error);

    if (error != NULL)
    {
        nautilus_task_set_error (task, error);
        nautilus_task_complete (task);

        return;
    }

    hash_table = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                        g_object_unref, g_object_unref);

    do
    {
        GFile *child;

        info = g_file_enumerator_next_file (enumerator, cancellable, &error);

        if (error != NULL)
        {
            g_hash_table_destroy (hash_table);

            nautilus_task_set_error (task, error);
            nautilus_task_complete (task);

            return;
        }

        if (info != NULL)
        {
            child = g_file_enumerator_get_child (enumerator, info);

            g_assert (g_hash_table_insert (hash_table, child, info));
        }
    } while (info != NULL);

    nautilus_task_set_result (task, G_TYPE_HASH_TABLE, hash_table);
    nautilus_task_complete (task);
}

void
nautilus_load_pixbuf_func (NautilusTask *task,
                           gpointer      task_data)
{
    GError *error = NULL;
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_new_from_file (task_data, &error);

    nautilus_task_set_error (task, error);
    nautilus_task_set_result (task, GDK_TYPE_PIXBUF, pixbuf);
    nautilus_task_complete (task);
}

void
nautilus_rename_task_func (NautilusTask *task,
                           gpointer      task_data)
{
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;

    g_hash_table_iter_init (&iter, (GHashTable *) task_data);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        GFile *location_from;
        GFile *location_to;

        location_from = G_FILE (key);
        location_to = g_file_set_display_name (location_from,
                                               (const gchar *) value,
                                               cancellable, &error);

        if (location_to != NULL)
        {
            nautilus_notify_file_renamed (location_from, location_to);
        }
        else
        {
        }
    }

    nautilus_task_complete (task);
}

static gpointer
create_thumbnail_factory (gpointer data)
{
    (void) data;

    return gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
}

static GnomeDesktopThumbnailFactory *
get_thumbnail_factory (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, create_thumbnail_factory, NULL);

    return once.retval;
}

void
nautilus_thumbnail_task_func (NautilusTask *task,
                              gpointer      task_data)
{
    GFile *location;
    GnomeDesktopThumbnailFactory *thumbnail_factory;
    g_autofree gchar *uri = NULL;
    g_autoptr (GFileInfo) file_info = NULL;
    const gchar *content_type;
    guint64 mtime;
    GdkPixbuf *pixbuf;

    location = task_data;
    thumbnail_factory = get_thumbnail_factory ();
    uri = g_file_get_uri (location);
    file_info = g_file_query_info (location,
                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                   G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL, NULL);
    content_type = g_file_info_get_content_type (file_info);
    mtime = g_file_info_get_attribute_uint64 (file_info,
                                              G_FILE_ATTRIBUTE_TIME_MODIFIED);

    if (!gnome_desktop_thumbnail_factory_can_thumbnail (thumbnail_factory,
                                                        uri, content_type,
                                                        mtime))
    {
        nautilus_task_complete (task);
        return;
    }

    pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory,
                                                                 uri,
                                                                 content_type);

    if (pixbuf != NULL)
    {
        gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory,
                                                        pixbuf, uri, mtime);
    }
    else
    {
        gnome_desktop_thumbnail_factory_create_failed_thumbnail (thumbnail_factory,
                                                                 uri, mtime);
    }

    nautilus_task_set_result (task, GDK_TYPE_PIXBUF, g_object_ref (pixbuf));
    nautilus_task_complete (task);
}

void
nautilus_query_info_func (NautilusTask *task,
                          gpointer      task_data)
{
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;
    GFileInfo *info;

    cancellable = nautilus_task_get_cancellable (task);
    info = g_file_query_info (task_data,
                              DEFAULT_ATTRIBUTES,
                              G_FILE_QUERY_INFO_NONE,
                              cancellable,
                              &error);

    nautilus_task_set_error (task, error);
    nautilus_task_set_result (task, G_TYPE_FILE_INFO, info);
    nautilus_task_complete (task);
}
