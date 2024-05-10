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
#define G_LOG_DOMAIN "nautilus-thumbnails"

#include <config.h>
#include "nautilus-thumbnails.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "nautilus-directory-notify.h"
#include "nautilus-global-preferences.h"
#include "nautilus-file-utilities.h"
#include "nautilus-hash-queue.h"
#include <math.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "nautilus-file-private.h"

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

/* Cool-off period between last file modification time and thumbnail creation */
#define THUMBNAIL_CREATION_DELAY_SECS 3

/* This specific number of processors seems to work ok even on relatively slow
 * computers. However, this might not be the effective number of processors
 * used simultaneously because of main thread load and I/O bounds. */
#define MAX_THUMBNAILING_THREADS ceil (g_get_num_processors () / 2);

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
static NautilusHashQueue *thumbnails_to_make = NULL;

/* The icons being currently thumbnailed. */
static GHashTable *currently_thumbnailing_hash = NULL;

/* The number of currently running threads. */
static guint running_threads = 0;

/* The maximum number of threads allowed. */
static guint max_threads = 0;

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
    if (info != NULL)
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
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusThumbnailInfo, free_thumbnail_info)

static gpointer
create_info_key (gpointer item)
{
    NautilusThumbnailInfo *info = item;
    return info->image_uri;
}

static GnomeDesktopThumbnailFactory *
get_thumbnail_factory (void)
{
    static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;

    if (G_UNLIKELY (thumbnail_factory == NULL))
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
    NautilusThumbnailInfo *info;

    if (G_UNLIKELY (thumbnails_to_make == NULL))
    {
        return;
    }

    nautilus_hash_queue_remove (thumbnails_to_make, file_uri);

    info = g_hash_table_lookup (currently_thumbnailing_hash, file_uri);
    if (info != NULL)
    {
        g_cancellable_cancel (info->cancellable);
    }
}

void
nautilus_thumbnail_prioritize (const char *file_uri)
{
    if (G_UNLIKELY (thumbnails_to_make == NULL))
    {
        return;
    }

    nautilus_hash_queue_move_existing_to_head (thumbnails_to_make, file_uri);
}

void
nautilus_thumbnail_deprioritize (const char *file_uri)
{
    if (G_UNLIKELY (thumbnails_to_make == NULL))
    {
        return;
    }

    nautilus_hash_queue_move_existing_to_tail (thumbnails_to_make, file_uri);
}

/***************************************************************************
 * Thumbnail Thread Functions.
 ***************************************************************************/

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
    const char *mime_type = nautilus_file_get_mime_type (file);

    uri = nautilus_file_get_uri (file);
    mtime = nautilus_file_get_mtime (file);

    factory = get_thumbnail_factory ();
    res = gnome_desktop_thumbnail_factory_can_thumbnail (factory,
                                                         uri,
                                                         mime_type,
                                                         mtime);
    g_free (uri);

    return res;
}

void
nautilus_create_thumbnail (NautilusFile *file)
{
    time_t file_mtime = 0;

    nautilus_file_set_is_thumbnailing (file, TRUE);

    g_autoptr (NautilusThumbnailInfo) info = g_new0 (NautilusThumbnailInfo, 1);
    info->image_uri = nautilus_file_get_uri (file);
    info->mime_type = g_strdup (nautilus_file_get_mime_type (file));
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

    if (G_UNLIKELY (thumbnails_to_make == NULL))
    {
        thumbnails_to_make = nautilus_hash_queue_new (g_str_hash, g_str_equal, create_info_key, NULL);
        currently_thumbnailing_hash = g_hash_table_new (g_str_hash,
                                                        g_str_equal);
    }

    /* Check if it is already in the list of thumbnails to make or
     *  currently being made. */
    NautilusThumbnailInfo *existing_info = g_hash_table_lookup (currently_thumbnailing_hash, info->image_uri);

    if (existing_info == NULL)
    {
        existing_info = nautilus_hash_queue_find_item (thumbnails_to_make, info->image_uri);
    }

    if (existing_info == NULL)
    {
        /* Add the thumbnail to the list. */
        g_debug ("(Main Thread) Adding thumbnail: %s",
                 info->image_uri);
        nautilus_hash_queue_enqueue (thumbnails_to_make, g_steal_pointer (&info));

        /* If we didn't schedule the thumbnail function to start on idle, do
         *  that now. We don't want to start it until all the other work is
         *  done, so the GUI will be updated as quickly as possible. */
        if (thumbnail_thread_starter_id == 0)
        {
            thumbnail_thread_starter_id = g_idle_add_full (G_PRIORITY_LOW, thumbnail_starter_cb, NULL, NULL);
        }
    }
    else
    {
        g_debug ("(Main Thread) Updating non-current mtime: %s",
                 info->image_uri);

        /* The file in the queue might need a new original mtime */
        existing_info->updated_file_mtime = info->original_file_mtime;
    }
}

static void
thumbnail_finalize (NautilusThumbnailInfo *info)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (info->image_uri);

    nautilus_file_set_is_thumbnailing (file, FALSE);
    g_hash_table_remove (currently_thumbnailing_hash, info->image_uri);
    running_threads -= 1;

    /*  If the original file mtime of the request changed, then
     *  we need to redo the thumbnail. */
    if (info->original_file_mtime == info->updated_file_mtime ||
        g_cancellable_is_cancelled (info->cancellable))
    {
        free_thumbnail_info (info);
    }
    else
    {
        info->original_file_mtime = info->updated_file_mtime;

        nautilus_hash_queue_enqueue (thumbnails_to_make, info);
    }

    if (nautilus_hash_queue_is_empty (thumbnails_to_make))
    {
        g_debug ("(Thumbnail Async Thread) Exiting");
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
        g_debug ("(Thumbnail Async Thread) Could not create a failed thumbnail: %s (%s)",
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
        g_debug ("(Thumbnail Async Thread) Saving thumbnail failed: %s (%s)",
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
    g_autoptr (NautilusFile) file = NULL;

    pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail_finish (thumbnail_factory,
                                                                        result,
                                                                        &error);

    if (g_cancellable_is_cancelled (info->cancellable))
    {
        g_debug ("(Thumbnail Async Thread) Cancelled thumbnail: %s",
                 info->image_uri);

        thumbnail_finalize (info);
        return;
    }

    file = nautilus_file_get_by_uri (info->image_uri);

    if (pixbuf != NULL)
    {
        g_autofree gchar *mtime = g_strdup_printf ("%" G_GINT64_FORMAT,
                                                   (gint64) info->updated_file_mtime);

        g_debug ("(Thumbnail Async Thread) Saving thumbnail: %s",
                 info->image_uri);

        /* This is needed since the attribute is not set on the pixbuf,
         *  only the written thumbnail file.
         */
        gdk_pixbuf_set_option (pixbuf, "tEXt::Thumb::MTime", mtime);
        nautilus_file_set_thumbnail (file, pixbuf);

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
        g_debug ("(Thumbnail Async Thread) Thumbnail failed: %s (%s)",
                 info->image_uri, error->message);

        gnome_desktop_thumbnail_factory_create_failed_thumbnail_async (thumbnail_factory,
                                                                       info->image_uri,
                                                                       info->updated_file_mtime,
                                                                       info->cancellable,
                                                                       thumbnail_failed_cb,
                                                                       info);
    }

    nautilus_file_changed (file);
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
    guint ignored_thumbnails = 0;
    time_t current_orig_mtime = 0;
    time_t current_time;
    guint backoff_time;
    guint backoff_time_min = THUMBNAIL_CREATION_DELAY_SECS + 1;

    g_debug ("(Main Thread) Creating thumbnails thread");

    thumbnail_factory = get_thumbnail_factory ();
    thumbnail_thread_starter_id = 0;

    if (G_UNLIKELY (max_threads == 0))
    {
        max_threads = MAX_THUMBNAILING_THREADS
    }

    /* We loop until the queue is empty, or we reach the thread limit. */
    while (ignored_thumbnails < nautilus_hash_queue_get_length (thumbnails_to_make) &&
           running_threads <= max_threads)
    {
        info = nautilus_hash_queue_peek_head (thumbnails_to_make);
        nautilus_hash_queue_remove (thumbnails_to_make, info->image_uri);

        current_orig_mtime = info->updated_file_mtime;
        time (&current_time);

        /* Don't try to create a thumbnail if the file was modified recently.
         *  This prevents constant re-thumbnailing of changing files. */
        if (current_time < current_orig_mtime + THUMBNAIL_CREATION_DELAY_SECS &&
            current_time >= current_orig_mtime)
        {
            g_debug ("(Thumbnail Thread) Skipping: %s",
                     info->image_uri);

            /* Only retain the smallest backoff time */
            backoff_time = THUMBNAIL_CREATION_DELAY_SECS - (current_time - current_orig_mtime);
            backoff_time_min = MIN (backoff_time, backoff_time_min);

            nautilus_hash_queue_enqueue (thumbnails_to_make, info);
            ignored_thumbnails += 1;
            continue;
        }

        /* Create the thumbnail. */
        g_debug ("(Thumbnail Thread) Creating thumbnail: %s",
                 info->image_uri);

        running_threads += 1;
        g_hash_table_insert (currently_thumbnailing_hash, info->image_uri, info);

        gnome_desktop_thumbnail_factory_generate_thumbnail_async (thumbnail_factory,
                                                                  info->image_uri,
                                                                  info->mime_type,
                                                                  info->cancellable,
                                                                  thumbnail_generated_cb,
                                                                  info);
    }

    /* Reschedule thumbnailing via a change notification */
    if (thumbnail_thread_starter_id == 0 &&
        ignored_thumbnails > 0)
    {
        thumbnail_thread_starter_id = g_timeout_add_seconds (backoff_time_min,
                                                             thumbnail_starter_cb, NULL);
    }

    return G_SOURCE_REMOVE;
}
