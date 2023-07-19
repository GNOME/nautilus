/*
 *  nautilus-thumbnails.h: Thumbnail code for icon factory.
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2002, 2003 Red Hat, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 */

#include <config.h>
#include "nautilus-thumbnails.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "nautilus-directory-notify.h"
#include "nautilus-global-preferences.h"
#include "nautilus-file-utilities.h"
#include <math.h>
#include <eel/eel-string.h>
#include <eel/eel-debug.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#define DEBUG_FLAG NAUTILUS_DEBUG_THUMBNAILS
#include "nautilus-debug.h"

#include "nautilus-file-private.h"

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

/* Cool-off period between last file modification time and thumbnail creation */
#define THUMBNAIL_CREATION_DELAY_SECS 3

static gboolean thumbnail_starter_cb (gpointer data);

/* structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */

typedef struct
{
    char *image_uri;
    char *mime_type;
    time_t original_file_mtime;
    time_t updated_file_mtime;

    GCancellable *cancellable;
} NautilusThumbnailInfo;

/*
 * Thumbnail thread state.
 */

/* The id of the idle handler used to start the thumbnail thread, or 0 if no
 *  idle handler is currently registered. */
static guint thumbnail_thread_starter_id = 0;

/* The list of NautilusThumbnailInfo structs containing information about the
 *  thumbnails we are making. */
static volatile GQueue thumbnails_to_make = G_QUEUE_INIT;

/* Quickly check if uri is in thumbnails_to_make list */
static GHashTable *thumbnails_to_make_hash = NULL;

/* The currently thumbnailed icon. it also exists in the thumbnails_to_make list
 * to avoid adding it again. */
static NautilusThumbnailInfo *currently_thumbnailing = NULL;

static gboolean
get_file_mtime (const char *file_uri,
                time_t     *mtime)
{
    GFile *file;
    GFileInfo *info;
    gboolean ret;

    ret = FALSE;
    *mtime = INVALID_MTIME;

    file = g_file_new_for_uri (file_uri);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
    if (info)
    {
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
        {
            *mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
            ret = TRUE;
        }

        g_object_unref (info);
    }
    g_object_unref (file);

    return ret;
}

static void
free_thumbnail_info (NautilusThumbnailInfo *info)
{
    g_free (info->image_uri);
    g_free (info->mime_type);
    g_clear_object (&info->cancellable);
    g_free (info);
}

static GnomeDesktopThumbnailFactory *
get_thumbnail_factory (void)
{
    static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;

    if (thumbnail_factory == NULL)
    {
        GdkDisplay *display = gdk_display_get_default ();
        GListModel *monitors = gdk_display_get_monitors (display);
        gint max_scale = 1;
        GnomeDesktopThumbnailSize size;

        for (guint i = 0; i < g_list_model_get_n_items (monitors); i++)
        {
            g_autoptr (GdkMonitor) monitor = g_list_model_get_item (monitors, i);

            max_scale = MAX (max_scale, gdk_monitor_get_scale_factor (monitor));
        }

        if (max_scale <= 1)
        {
            size = GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE;
        }
        else if (max_scale <= 2)
        {
            size = GNOME_DESKTOP_THUMBNAIL_SIZE_XLARGE;
        }
        else
        {
            size = GNOME_DESKTOP_THUMBNAIL_SIZE_XXLARGE;
        }

        thumbnail_factory = gnome_desktop_thumbnail_factory_new (size);
    }

    return thumbnail_factory;
}

void
nautilus_thumbnail_remove_from_queue (const char *file_uri)
{
    GList *node;

    if (thumbnails_to_make_hash)
    {
        node = g_hash_table_lookup (thumbnails_to_make_hash, file_uri);
        if (node == NULL)
        {
            return;
        }

        if (node->data == currently_thumbnailing)
        {
            NautilusThumbnailInfo *info = node->data;

            g_cancellable_cancel (info->cancellable);
        }
        else
        {
            g_hash_table_remove (thumbnails_to_make_hash, file_uri);
            free_thumbnail_info (node->data);
            g_queue_delete_link ((GQueue *) &thumbnails_to_make, node);
        }
    }
}

void
nautilus_thumbnail_prioritize (const char *file_uri)
{
    GList *node;

    if (thumbnails_to_make_hash)
    {
        node = g_hash_table_lookup (thumbnails_to_make_hash, file_uri);

        if (node && node->data != currently_thumbnailing)
        {
            g_queue_unlink ((GQueue *) &thumbnails_to_make, node);
            g_queue_push_head_link ((GQueue *) &thumbnails_to_make, node);
        }
    }
}


/***************************************************************************
 * Thumbnail Thread Functions.
 ***************************************************************************/


static gboolean
thumbnail_thread_notify_file_changed (gpointer image_uri)
{
    NautilusFile *file;

    file = nautilus_file_get_by_uri ((char *) image_uri);

    DEBUG ("(Thumbnail Thread) Notifying file changed file:%p uri: %s\n", file, (char *) image_uri);

    if (file != NULL)
    {
        nautilus_file_set_is_thumbnailing (file, FALSE);
        nautilus_file_invalidate_attributes (file,
                                             NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL |
                                             NAUTILUS_FILE_ATTRIBUTE_INFO);
        nautilus_file_unref (file);
    }
    g_free (image_uri);

    return FALSE;
}

static GHashTable *
get_types_table (void)
{
    static GHashTable *image_mime_types = NULL;
    GSList *format_list, *l;
    char **types;
    int i;

    if (image_mime_types == NULL)
    {
        image_mime_types =
            g_hash_table_new_full (g_str_hash, g_str_equal,
                                   g_free, NULL);

        format_list = gdk_pixbuf_get_formats ();
        for (l = format_list; l; l = l->next)
        {
            types = gdk_pixbuf_format_get_mime_types (l->data);

            for (i = 0; types[i] != NULL; i++)
            {
                g_hash_table_insert (image_mime_types,
                                     types [i],
                                     GUINT_TO_POINTER (1));
            }

            g_free (types);
        }

        g_slist_free (format_list);
    }

    return image_mime_types;
}

static gboolean
pixbuf_can_load_type (const char *mime_type)
{
    GHashTable *image_mime_types;

    image_mime_types = get_types_table ();
    if (g_hash_table_lookup (image_mime_types, mime_type))
    {
        return TRUE;
    }

    return FALSE;
}

gboolean
nautilus_thumbnail_is_mimetype_limited_by_size (const char *mime_type)
{
    return pixbuf_can_load_type (mime_type);
}

gboolean
nautilus_can_thumbnail (NautilusFile *file)
{
    GnomeDesktopThumbnailFactory *factory;
    gboolean res;
    char *uri;
    time_t mtime;
    char *mime_type;

    uri = nautilus_file_get_uri (file);
    mime_type = nautilus_file_get_mime_type (file);
    mtime = nautilus_file_get_mtime (file);

    factory = get_thumbnail_factory ();
    res = gnome_desktop_thumbnail_factory_can_thumbnail (factory,
                                                         uri,
                                                         mime_type,
                                                         mtime);
    g_free (mime_type);
    g_free (uri);

    return res;
}

void
nautilus_create_thumbnail (NautilusFile *file)
{
    time_t file_mtime = 0;
    NautilusThumbnailInfo *info;
    NautilusThumbnailInfo *existing_info;
    GList *existing, *node;

    nautilus_file_set_is_thumbnailing (file, TRUE);

    info = g_new0 (NautilusThumbnailInfo, 1);
    info->image_uri = nautilus_file_get_uri (file);
    info->mime_type = nautilus_file_get_mime_type (file);
    info->cancellable = g_cancellable_new ();

    /* Hopefully the NautilusFile will already have the image file mtime,
     *  so we can just use that. Otherwise we have to get it ourselves. */
    if (file->details->got_file_info &&
        file->details->file_info_is_up_to_date &&
        file->details->mtime != 0)
    {
        file_mtime = file->details->mtime;
    }
    else
    {
        get_file_mtime (info->image_uri, &file_mtime);
    }

    info->original_file_mtime = file_mtime;
    info->updated_file_mtime = file_mtime;

    if (thumbnails_to_make_hash == NULL)
    {
        thumbnails_to_make_hash = g_hash_table_new (g_str_hash,
                                                    g_str_equal);
    }

    /* Check if it is already in the list of thumbnails to make. */
    existing = g_hash_table_lookup (thumbnails_to_make_hash, info->image_uri);
    if (existing == NULL)
    {
        /* Add the thumbnail to the list. */
        DEBUG ("(Main Thread) Adding thumbnail: %s\n",
               info->image_uri);
        g_queue_push_tail ((GQueue *) &thumbnails_to_make, info);
        node = g_queue_peek_tail_link ((GQueue *) &thumbnails_to_make);
        g_hash_table_insert (thumbnails_to_make_hash,
                             info->image_uri,
                             node);
        /* If the thumbnail thread isn't running, and we haven't
         *  scheduled an idle function to start it up, do that now.
         *  We don't want to start it until all the other work is done,
         *  so the GUI will be updated as quickly as possible.*/
        if (currently_thumbnailing == NULL &&
            thumbnail_thread_starter_id == 0)
        {
            thumbnail_thread_starter_id = g_idle_add_full (G_PRIORITY_LOW, thumbnail_starter_cb, NULL, NULL);
        }
    }
    else
    {
        DEBUG ("(Main Thread) Updating non-current mtime: %s\n",
               info->image_uri);

        /* The file in the queue might need a new original mtime */
        existing_info = existing->data;
        existing_info->updated_file_mtime = info->original_file_mtime;
        free_thumbnail_info (info);
    }
}

static void
thumbnail_finalize (NautilusThumbnailInfo *info)
{
    GList *node;

    thumbnail_thread_notify_file_changed (g_strdup (info->image_uri));
    currently_thumbnailing = NULL;

    /* Pop the last thumbnail we just made off the head of the
     *  list and free it. Don't pop the thumbnail off the queue if the
     *  original file mtime of the request changed. Then we need to
     *  redo the thumbnail.
     */
    if (info->original_file_mtime == info->updated_file_mtime ||
        g_cancellable_is_cancelled (info->cancellable))
    {
        node = g_hash_table_lookup (thumbnails_to_make_hash, info->image_uri);
        g_hash_table_remove (thumbnails_to_make_hash, info->image_uri);
        free_thumbnail_info (info);
        g_queue_delete_link ((GQueue *) &thumbnails_to_make, node);
    }
    else
    {
        info->original_file_mtime = info->updated_file_mtime;
    }

    if (g_queue_is_empty ((GQueue *) &thumbnails_to_make))
    {
        DEBUG ("(Thumbnail Async Thread) Exiting\n");
    }
    else if (thumbnail_thread_starter_id == 0)
    {
        thumbnail_thread_starter_id = g_idle_add (thumbnail_starter_cb, NULL);
    }
}

static void
thumbnail_failed_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      data)
{
    GnomeDesktopThumbnailFactory *thumbnail_factory = GNOME_DESKTOP_THUMBNAIL_FACTORY (source_object);
    NautilusThumbnailInfo *info = data;
    g_autoptr (GError) error = NULL;

    gnome_desktop_thumbnail_factory_create_failed_thumbnail_finish (thumbnail_factory,
                                                                    result,
                                                                    &error);
    if (error != NULL)
    {
        DEBUG ("(Thumbnail Async Thread) Could not create a failed thumbnail: %s (%s)\n",
               info->image_uri, error->message);
    }

    thumbnail_finalize (info);
}

static void
thumbnail_saved_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      data)
{
    GnomeDesktopThumbnailFactory *thumbnail_factory = GNOME_DESKTOP_THUMBNAIL_FACTORY (source_object);
    NautilusThumbnailInfo *info = data;
    g_autoptr (GError) error = NULL;

    gnome_desktop_thumbnail_factory_save_thumbnail_finish (thumbnail_factory,
                                                           result,
                                                           &error);
    if (error != NULL)
    {
        DEBUG ("(Thumbnail Async Thread) Saving thumbnail failed: %s (%s)\n",
               info->image_uri, error->message);
    }

    thumbnail_finalize (info);
}

static void
thumbnail_generated_cb (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      data)
{
    GnomeDesktopThumbnailFactory *thumbnail_factory = GNOME_DESKTOP_THUMBNAIL_FACTORY (source_object);
    NautilusThumbnailInfo *info = data;
    g_autoptr (GError) error = NULL;
    g_autoptr (GdkPixbuf) pixbuf = NULL;

    pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail_finish (thumbnail_factory,
                                                                        result,
                                                                        &error);

    if (g_cancellable_is_cancelled (info->cancellable))
    {
        DEBUG ("(Thumbnail Async Thread) Cancelled thumbnail: %s\n",
               info->image_uri);

        thumbnail_finalize (info);
        return;
    }

    if (pixbuf != NULL)
    {
        DEBUG ("(Thumbnail Async Thread) Saving thumbnail: %s\n",
               info->image_uri);

        gnome_desktop_thumbnail_factory_save_thumbnail_async (thumbnail_factory,
                                                              pixbuf,
                                                              info->image_uri,
                                                              info->updated_file_mtime,
                                                              info->cancellable,
                                                              thumbnail_saved_cb,
                                                              info);
    }
    else
    {
        DEBUG ("(Thumbnail Async Thread) Thumbnail failed: %s (%s)\n",
               info->image_uri, error->message);

        gnome_desktop_thumbnail_factory_create_failed_thumbnail_async (thumbnail_factory,
                                                                       info->image_uri,
                                                                       info->updated_file_mtime,
                                                                       info->cancellable,
                                                                       thumbnail_failed_cb,
                                                                       info);
    }
}

/* This function is added as a very low priority idle function to start the
 *  async threads to create any needed thumbnails. It is added with a very
 *  low priority so that it doesn't delay showing the directory in the
 *  icon/list views. We want to show the files in the directory as quickly
 *  as possible. */
static gboolean
thumbnail_starter_cb (gpointer data)
{
    GnomeDesktopThumbnailFactory *thumbnail_factory;
    NautilusThumbnailInfo *info = NULL;
    time_t current_orig_mtime = 0;
    time_t current_time;

    DEBUG ("(Main Thread) Creating thumbnails thread\n");

    thumbnail_factory = get_thumbnail_factory ();
    thumbnail_thread_starter_id = 0;

    /* We loop until the queue is empty, or if we already
     * have a thumbnail in progress.
     */
    while (!g_queue_is_empty ((GQueue *) &thumbnails_to_make) &&
           currently_thumbnailing == NULL)
    {
        info = g_queue_peek_head ((GQueue *) &thumbnails_to_make);
        current_orig_mtime = info->updated_file_mtime;

        time (&current_time);

        /* Don't try to create a thumbnail if the file was modified recently.
         *  This prevents constant re-thumbnailing of changing files. */
        if (current_time < current_orig_mtime + THUMBNAIL_CREATION_DELAY_SECS &&
            current_time >= current_orig_mtime)
        {
            DEBUG ("(Thumbnail Thread) Skipping: %s\n",
                   info->image_uri);

            /* Reschedule thumbnailing via a change notification */
            g_timeout_add_seconds (1, thumbnail_thread_notify_file_changed,
                                   g_strdup (info->image_uri));
            nautilus_thumbnail_remove_from_queue (info->image_uri);
            continue;
        }

        /* Create the thumbnail. */
        DEBUG ("(Thumbnail Thread) Creating thumbnail: %s\n",
               info->image_uri);

        currently_thumbnailing = info;

        gnome_desktop_thumbnail_factory_generate_thumbnail_async (thumbnail_factory,
                                                                  info->image_uri,
                                                                  info->mime_type,
                                                                  info->cancellable,
                                                                  thumbnail_generated_cb,
                                                                  info);
    }

    return G_SOURCE_REMOVE;
}
