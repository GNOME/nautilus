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
    GCancellable *cancellable;
    GAsyncReadyCallback callback;
    gpointer user_data;
} ThumbnailCreationCallback;

typedef struct
{
    char *image_uri;
    char *mime_type;
    time_t original_file_mtime;
    time_t updated_file_mtime;
    GdkPixbuf *pixbuf;
    GPtrArray *callbacks;

    GError *error;
} NautilusThumbnailInfo;

typedef struct
{
    NautilusThumbnailInfo *info;
    ThumbnailCreationCallback *callback;
} ThumbnailCreationResult;

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
free_thumbnail_callback (ThumbnailCreationCallback *cb_data)
{
    g_clear_object (&cb_data->cancellable);
    g_free (cb_data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ThumbnailCreationCallback, free_thumbnail_callback)

static void
free_thumbnail_info (NautilusThumbnailInfo *info)
{
    g_free (info->image_uri);
    g_free (info->mime_type);
    g_clear_object (&info->pixbuf);
    g_clear_pointer (&info->callbacks, g_ptr_array_unref);
    g_clear_error (&info->error);
    g_free (info);
}
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusThumbnailInfo, free_thumbnail_info)

static GnomeDesktopThumbnailSize
get_thumbnail_scale (void)
{
    static gint max_scale = 0;
    static GnomeDesktopThumbnailSize size;

    if (G_UNLIKELY (max_scale == 0))
    {
        GdkDisplay *display = gdk_display_get_default ();
        GListModel *monitors = gdk_display_get_monitors (display);
        max_scale = 1;

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
    }

    return size;
}

static GnomeDesktopThumbnailFactory *
get_thumbnail_factory (void)
{
    static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;

    if (G_UNLIKELY (thumbnail_factory == NULL))
    {
        GnomeDesktopThumbnailSize size = get_thumbnail_scale ();

        thumbnail_factory = gnome_desktop_thumbnail_factory_new (size);
    }

    return thumbnail_factory;
}

guint
nautilus_thumbnail_get_max_size (void)
{
    GnomeDesktopThumbnailSize size = get_thumbnail_scale ();

    switch (size)
    {
        case GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL:
        {
            return 128;
        }

        case GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE:
        {
            return 256;
        }

        case GNOME_DESKTOP_THUMBNAIL_SIZE_XLARGE:
        {
            return 512;
        }

        case GNOME_DESKTOP_THUMBNAIL_SIZE_XXLARGE:
        {
            return 1024;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }
}

char *
nautilus_thumbnail_get_path_for_uri (const char *uri)
{
    return gnome_desktop_thumbnail_path_for_uri (uri, get_thumbnail_scale ());
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
nautilus_can_thumbnail (const gchar *uri,
                        const gchar *mime_type,
                        time_t       modified_time)
{
    GnomeDesktopThumbnailFactory *factory = get_thumbnail_factory ();

    return gnome_desktop_thumbnail_factory_can_thumbnail (factory,
                                                          uri,
                                                          mime_type,
                                                          modified_time);
}

static void
handle_cancelled_callbacks (NautilusThumbnailInfo *info)
{
    for (guint i = 0; i < info->callbacks->len; i++)
    {
        ThumbnailCreationCallback *thumbnail_callback = info->callbacks->pdata[i];
        ThumbnailCreationResult res = { .info = info, .callback = thumbnail_callback };

        if (thumbnail_callback->cancellable != NULL &&
            g_cancellable_is_cancelled (thumbnail_callback->cancellable))
        {
            g_debug ("Cancelled thumbnail: %s", info->image_uri);

            if (thumbnail_callback->callback != NULL)
            {
                (*thumbnail_callback->callback) (NULL,
                                                 (GAsyncResult *) &res,
                                                 thumbnail_callback->user_data);
            }

            g_ptr_array_remove_index_fast (info->callbacks, i--);
        }
    }
}

static void
handle_callbacks_and_free (NautilusThumbnailInfo *info)
{
    for (uint i = 0; i < info->callbacks->len; i++)
    {
        ThumbnailCreationCallback *thumbnail_callback = info->callbacks->pdata[i];
        ThumbnailCreationResult res = { .info = info, .callback = thumbnail_callback };

        if (thumbnail_callback->callback != NULL)
        {
            (*thumbnail_callback->callback) (NULL,
                                             (GAsyncResult *) &res,
                                             thumbnail_callback->user_data);
        }
    }

    free_thumbnail_info (info);
}

void
nautilus_create_thumbnail_async (const gchar         *uri,
                                 const gchar         *mime_type,
                                 time_t               modified_time,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
    g_return_if_fail (uri != NULL && *uri != '\0');

    g_autoptr (NautilusThumbnailInfo) info = g_new0 (NautilusThumbnailInfo, 1);
    g_autoptr (ThumbnailCreationCallback) cb_data = g_new0 (ThumbnailCreationCallback, 1);

    info->image_uri = g_strdup (uri);
    info->mime_type = g_strdup (mime_type);
    info->callbacks = g_ptr_array_new_with_free_func ((GDestroyNotify) free_thumbnail_callback);

    cb_data->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;
    cb_data->callback = callback;
    cb_data->user_data = user_data;

    if (cancellable != NULL &&
        g_cancellable_is_cancelled (cancellable))
    {
        /* Call the callback immediately */
        g_ptr_array_add (info->callbacks, g_steal_pointer (&cb_data));
        handle_cancelled_callbacks (info);

        return;
    }

    /* Hopefully the caller will already have the image file mtime,
     *  so we can just use that. Otherwise we have to get it ourselves. */
    if (modified_time == 0)
    {
        get_file_mtime (info->image_uri, &modified_time);
    }

    info->original_file_mtime = modified_time;
    info->updated_file_mtime = modified_time;

    if (G_UNLIKELY (thumbnails_to_make == NULL))
    {
        thumbnails_to_make = nautilus_hash_queue_new (g_str_hash, g_str_equal, NULL, NULL);
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

        g_ptr_array_add (info->callbacks, g_steal_pointer (&cb_data));
        nautilus_hash_queue_enqueue (thumbnails_to_make, info->image_uri, info);
        g_steal_pointer (&info);

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
        g_ptr_array_add (existing_info->callbacks, g_steal_pointer (&cb_data));
    }
}

GdkPixbuf *
nautilus_create_thumbnail_finish (GAsyncResult  *res,
                                  GError       **error)
{
    ThumbnailCreationResult *result = (ThumbnailCreationResult *) res;
    ThumbnailCreationCallback *callback = result->callback;
    NautilusThumbnailInfo *info = result->info;

    if (callback->cancellable != NULL &&
        g_cancellable_is_cancelled (callback->cancellable))
    {
        if (error != NULL)
        {
            *error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
        }

        return NULL;
    }

    if (info->error != NULL)
    {
        if (error != NULL)
        {
            *error = g_error_copy (info->error);
        }

        return NULL;
    }

    return info->pixbuf != NULL ? g_object_ref (info->pixbuf) : NULL;
}

static void
thumbnail_finalize (NautilusThumbnailInfo *info)
{
    g_hash_table_remove (currently_thumbnailing_hash, info->image_uri);
    running_threads -= 1;

    handle_cancelled_callbacks (info);

    /*  If the original file mtime of the request changed, then
     *  we need to redo the thumbnail. */
    if (info->original_file_mtime == info->updated_file_mtime ||
        info->callbacks->len == 0)
    {
        handle_callbacks_and_free (info);
    }
    else
    {
        info->original_file_mtime = info->updated_file_mtime;

        nautilus_hash_queue_enqueue (thumbnails_to_make, info->image_uri, info);
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
    GdkPixbuf *pixbuf = NULL;

    pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail_finish (thumbnail_factory,
                                                                        result,
                                                                        &error);

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
        info->pixbuf = pixbuf;

        gnome_desktop_thumbnail_factory_save_thumbnail_async (thumbnail_factory,
                                                              pixbuf,
                                                              info->image_uri,
                                                              info->updated_file_mtime,
                                                              NULL,
                                                              thumbnail_saved_cb,
                                                              info);
    }
    else
    {
        info->error = g_error_copy (error);
        g_debug ("(Thumbnail Async Thread) Thumbnail failed: %s (%s)",
                 info->image_uri, error->message);

        gnome_desktop_thumbnail_factory_create_failed_thumbnail_async (thumbnail_factory,
                                                                       info->image_uri,
                                                                       info->updated_file_mtime,
                                                                       NULL,
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

        handle_cancelled_callbacks (info);

        if (info->callbacks->len == 0)
        {
            free_thumbnail_info (info);

            continue;
        }

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

            nautilus_hash_queue_enqueue (thumbnails_to_make, info->image_uri, info);
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
                                                                  NULL,
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
