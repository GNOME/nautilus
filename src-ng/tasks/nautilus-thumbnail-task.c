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

#include "nautilus-thumbnail-task.h"

#include "nautilus-marshallers.h"
#include "nautilus-task-private.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#ifndef GNOME_DESKTOP_USE_UNSTABLE_API
#define GNOME_DESKTOP_USE_UNSTABLE_API
#endif

#include <libgnome-desktop/gnome-desktop-thumbnail.h>

struct _NautilusThumbnailTask
{
    NautilusTask parent_instance;

    GFile *location;
    gboolean use_external_thumbnailer;
};

G_DEFINE_TYPE (NautilusThumbnailTask, nautilus_thumbnail_task, NAUTILUS_TYPE_TASK)

enum
{
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
finalize (GObject *object)
{
    NautilusThumbnailTask *self;

    self = NAUTILUS_THUMBNAIL_TASK (object);

    g_object_unref (self->location);

    G_OBJECT_CLASS (nautilus_thumbnail_task_parent_class)->finalize (object);
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

static GdkPixbuf *
thumbnail_gnome_desktop (GFile *location)
{
    GnomeDesktopThumbnailFactory *thumbnail_factory;
    g_autofree gchar *uri = NULL;
    g_autoptr (GFileInfo) file_info = NULL;
    const gchar *content_type;
    guint64 mtime;
    GdkPixbuf *pixbuf;

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
        return NULL;
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

    return pixbuf;
}

static GdkPixbuf *
thumbnail_from_cache (GFile *location)
{
    g_autoptr (GFileInfo) file_info = NULL;
    gboolean thumbnail_is_valid;
    const char *thumbnail_path;

    file_info = g_file_query_info (location,
                                   G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                                   G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL, NULL);

    thumbnail_is_valid = g_file_info_get_attribute_boolean (file_info,
                                                            G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);

    if (!thumbnail_is_valid)
    {
        return NULL;
    }

    thumbnail_path = g_file_info_get_attribute_byte_string (file_info,
                                                            G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

    return gdk_pixbuf_new_from_file (thumbnail_path, NULL);
}

static gpointer
create_gdk_pixbuf_mime_types_table (gpointer data)
{
    GHashTable *hash_table;
    g_autoptr (GSList) gdk_pixbuf_formats = NULL;

    (void) data;

    hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    gdk_pixbuf_formats = gdk_pixbuf_get_formats ();

    for (GSList *i = gdk_pixbuf_formats; i != NULL; i = i->next)
    {
        g_autofree GStrv mime_types = NULL;

        mime_types = gdk_pixbuf_format_get_mime_types (i->data);

        for (gsize j = 0; mime_types[j] != NULL; j++)
        {
            g_hash_table_insert (hash_table,
                                 mime_types[j], GUINT_TO_POINTER (1));
        }
    }

    return hash_table;
}

static GHashTable *
get_gdk_pixbuf_mime_types (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, create_gdk_pixbuf_mime_types_table, NULL);

    return once.retval;
}

static GdkPixbuf *
thumbnail_gdk_pixbuf (GFile *location)
{
    GHashTable *gdk_pixbuf_mime_types;
    g_autoptr (GFileInfo) file_info = NULL;
    const gchar *content_type;

    gdk_pixbuf_mime_types = get_gdk_pixbuf_mime_types ();
    file_info = g_file_query_info (location,
                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                   G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL, NULL);
    content_type = g_file_info_get_content_type (file_info);

    if (g_hash_table_lookup (gdk_pixbuf_mime_types, content_type) == NULL)
    {
        return NULL;
    }

    return NULL;
}

static void
execute (NautilusTask *task)
{
    NautilusThumbnailTask *self;
    g_autoptr (GdkPixbuf) pixbuf = NULL;

    self = NAUTILUS_THUMBNAIL_TASK (task);

    if (self->use_external_thumbnailer)
    {
        pixbuf = thumbnail_from_cache (self->location);

        if (pixbuf == NULL)
        {
            pixbuf = thumbnail_gnome_desktop (self->location);
        }
    }
    else
    {
        pixbuf = thumbnail_gdk_pixbuf (self->location);
    }

    if (pixbuf == NULL)
    {
        pixbuf = gdk_pixbuf_new_from_resource ("/org/gnome/Nautilus/text-x-preview.png",
                                               NULL);
    }

    nautilus_task_emit_signal_in_main_context (task, signals[FINISHED], 0,
                                               self->location, pixbuf);
}

static void
nautilus_thumbnail_task_class_init (NautilusThumbnailTaskClass *klass)
{
    GObjectClass *object_class;
    NautilusTaskClass *task_class;

    object_class = G_OBJECT_CLASS (klass);
    task_class = NAUTILUS_TASK_CLASS (klass);

    object_class->finalize = finalize;

    task_class->execute = execute;

    signals[FINISHED] = g_signal_new ("finished",
                                      G_TYPE_FROM_CLASS (klass),
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL,
                                      nautilus_cclosure_marshal_VOID__OBJECT_OBJECT,
                                      G_TYPE_NONE,
                                      2,
                                      G_TYPE_FILE, GDK_TYPE_PIXBUF);
}

static void
nautilus_thumbnail_task_init (NautilusThumbnailTask *self)
{
}

NautilusTask *
nautilus_thumbnail_task_new (GFile    *location,
                             gboolean  use_external_thumbnailer)
{
    NautilusThumbnailTask *instance;

    g_return_val_if_fail (G_IS_FILE (location), NULL);

    instance = g_object_new (NAUTILUS_TYPE_THUMBNAIL_TASK, NULL);

    instance->location = g_object_ref (location);
    instance->use_external_thumbnailer = use_external_thumbnailer;

    return NAUTILUS_TASK (instance);
}
