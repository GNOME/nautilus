/*
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#define G_LOG_DOMAIN "nautilus-image"

#include "nautilus-image.h"

#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-hash-queue.h"
#include "nautilus-icon-info.h"
#include "nautilus-mime-actions.h"
#include "nautilus-scheme.h"
#include "nautilus-thumbnails.h"
#include "nautilus-ui-utilities.h"

#include <adwaita.h>
#include <gtk/gtk.h>

struct _NautilusImage
{
    GtkWidget parent_instance;

    gint size;
    GFile *source;
    GdkTexture *texture;
    guint64 source_mtime;
    gchar *source_content_type;
    GdkPaintable *fallback_paintable;

    gboolean is_loading_attributes;
    GCancellable *cancellable;
    GError *error;
};

enum
{
    PROP_0,
    PROP_SOURCE,
    PROP_SIZE,
    N_PROPS,
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (NautilusImage, nautilus_image, GTK_TYPE_WIDGET);

#define CACHE_COUNT_LIMIT 1000

static void
nautilus_image_set_texture (NautilusImage *self,
                            GdkTexture    *texture);

/* Global cache for all images. Maps GFile => GdkTexture */
static NautilusHashQueue *thumbnail_cache;

static guint64 cached_thumbnail_size_limit;

static void
update_thumbnail_limit (void)
{
    cached_thumbnail_size_limit = g_settings_get_uint64 (nautilus_preferences,
                                                         NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT);

    /* Converts the obtained limit in MB to bytes */
    cached_thumbnail_size_limit *= 1000000;
}

typedef struct
{
    GFile *file;
    GdkTexture *texture;
    guint64 mtime;
} ThumbnailCacheItem;

static void
thumbnail_cache_item_free (ThumbnailCacheItem *item)
{
    g_clear_object (&item->file);
    g_clear_object (&item->texture);
    g_free (item);
}

static ThumbnailCacheItem *
thumbnail_cache_get (GFile *file)
{
    if (thumbnail_cache == NULL)
    {
        return NULL;
    }

    ThumbnailCacheItem *item = nautilus_hash_queue_find_item (thumbnail_cache, file);

    if (item != NULL)
    {
        nautilus_hash_queue_move_existing_to_tail (thumbnail_cache, item->file);
    }

    return item;
}

static void
thumbnail_cache_add (GFile      *file,
                     GdkTexture *texture,
                     guint64     mtime)
{
    if (G_UNLIKELY (thumbnail_cache == NULL))
    {
        thumbnail_cache = nautilus_hash_queue_new (g_file_hash, (GEqualFunc) g_file_equal,
                                                   NULL, (GDestroyNotify) thumbnail_cache_item_free);
    }

    ThumbnailCacheItem *old_item = nautilus_hash_queue_find_item (thumbnail_cache, file);

    if (old_item != NULL)
    {
        g_set_object (&old_item->texture, texture);
        old_item->mtime = mtime;
        nautilus_hash_queue_move_existing_to_tail (thumbnail_cache, file);

        return;
    }
    else if (nautilus_hash_queue_get_length (thumbnail_cache) >= CACHE_COUNT_LIMIT)
    {
        ThumbnailCacheItem *item = nautilus_hash_queue_peek_head (thumbnail_cache);

        nautilus_hash_queue_remove (thumbnail_cache, item->file);
    }

    ThumbnailCacheItem *new_item = g_new0 (ThumbnailCacheItem, 1);
    new_item->file = g_object_ref (file);
    new_item->texture = g_object_ref (texture);
    new_item->mtime = mtime;

    nautilus_hash_queue_enqueue (thumbnail_cache, file, new_item);
}

#define LOADING_ICON_NAME "image-loading"
#define ERROR_ICON_NAME "image-missing"

static GdkPaintable *
get_paintable_for_themed_icon (NautilusImage *self,
                               const gchar   *icon_name)
{
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    g_autoptr (GIcon) icon = g_themed_icon_new (icon_name);
    GtkTextDirection direction = gtk_widget_get_direction (GTK_WIDGET (self));
    const int scale = 2;

    return GDK_PAINTABLE (gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                          icon,
                                                          nautilus_thumbnail_get_max_size (),
                                                          scale,
                                                          direction,
                                                          GTK_ICON_LOOKUP_PRELOAD));
}

static GdkPaintable *
get_loading_paintable (NautilusImage *self)
{
    static GdkPaintable *loading_paintable[3];
    int index = CLAMP (gtk_widget_get_scale_factor (GTK_WIDGET (self)), 1, 3) - 1;

    if (G_LIKELY (loading_paintable[index] != NULL))
    {
        return loading_paintable[index];
    }

    loading_paintable[index] = get_paintable_for_themed_icon (self, LOADING_ICON_NAME);

    return loading_paintable[index];
}

static GdkPaintable *
get_error_paintable (NautilusImage *self)
{
    static GdkPaintable *error_paintable[3];
    int index = CLAMP (gtk_widget_get_scale_factor (GTK_WIDGET (self)), 1, 3) - 1;

    if (G_LIKELY (error_paintable[index] != NULL))
    {
        return error_paintable[index];
    }

    error_paintable[index] = get_paintable_for_themed_icon (self, ERROR_ICON_NAME);

    return error_paintable[index];
}

static void
setup_texture_for_image (NautilusImage *self,
                         GdkPixbuf     *pixbuf)
{
    g_autoptr (GdkPixbuf) rotated_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
    g_autoptr (GdkTexture) texture = gdk_texture_new_for_pixbuf (rotated_pixbuf);

    nautilus_image_set_texture (self, texture);
    thumbnail_cache_add (self->source, self->texture, self->source_mtime);
}

static void
handle_loading_error (NautilusImage *self)
{
    /* Transition to error state */
    nautilus_image_set_texture (self, NULL);
    gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
thumbnailing_done_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      data)
{
    NautilusImage *self = data;
    g_autoptr (GError) error = NULL;
    g_autoptr (GdkPixbuf) pixbuf = nautilus_create_thumbnail_finish (res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The operation was cancelled and the file was disposed, bailout. */
        return;
    }

    self->error = g_steal_pointer (&error);

    if (pixbuf != NULL && self->error == NULL)
    {
        setup_texture_for_image (self, pixbuf);
    }
    else
    {
        handle_loading_error (self);
    }
}

static void
scale_down_when_large (GdkPixbuf **pixbuf)
{
    gint width = gdk_pixbuf_get_width (*pixbuf), height = gdk_pixbuf_get_height (*pixbuf);
    gint biggest_dimension = MAX (width, height);
    gint max_size = nautilus_thumbnail_get_max_size ();

    if (biggest_dimension <= max_size)
    {
        return;
    }

    gboolean wide = width > height;
    double scale = (double) max_size / (double) biggest_dimension;
    gint new_width = wide ? max_size : width * scale;
    gint new_height = wide ? height * scale : max_size;

    GdkPixbuf *new_pixbuf = gdk_pixbuf_scale_simple (*pixbuf,
                                                     new_width,
                                                     new_height,
                                                     GDK_INTERP_BILINEAR);
    g_clear_object (pixbuf);
    *pixbuf = new_pixbuf;
}

/* Currently, GDK Pixbuf will decode the image on the main thread, even when
 * using the async variant of the function. Until that is fixed, use a GTask to
 * perform the decoding in a different thread. */
static void
thumbnail_from_stream_thread (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
    GInputStream *self = source_object;
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream (self, cancellable, &error);

    if (pixbuf != NULL)
    {
        scale_down_when_large (&pixbuf);
        g_task_return_pointer (task, pixbuf, g_object_unref);
    }
    else
    {
        g_task_return_error (task, error);
    }
}

static void
thumbnail_from_stream_async (GInputStream        *stream,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    g_autoptr (GTask) task = g_task_new (stream, cancellable, callback, user_data);

    g_task_run_in_thread (task, thumbnail_from_stream_thread);
}

static GdkPixbuf *
thumbnail_from_stream_finish (GAsyncResult  *result,
                              GError       **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}

static void
thumbnail_pixbuf_ready_callback (GObject      *source_object,
                                 GAsyncResult *res,
                                 gpointer      user_data)
{
    g_autoptr (GError) error = NULL;
    NautilusImage *self = user_data;
    g_autoptr (GdkPixbuf) pixbuf = thumbnail_from_stream_finish (res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The operation was cancelled, bailout. */
        return;
    }

    self->error = g_steal_pointer (&error);

    if (pixbuf != NULL)
    {
        setup_texture_for_image (self, pixbuf);
    }
    else
    {
        handle_loading_error (self);
    }
}

static void
thumbnail_file_read_callback (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    NautilusImage *self = user_data;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInputStream) stream = g_file_read_finish (G_FILE (source_object), res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The operation was cancelled, bailout. */
        return;
    }

    self->error = g_steal_pointer (&error);

    if (stream != NULL && self->error == NULL)
    {
        thumbnail_from_stream_async (G_INPUT_STREAM (stream),
                                     self->cancellable,
                                     thumbnail_pixbuf_ready_callback,
                                     self);
    }
    else
    {
        handle_loading_error (self);
    }
}

static void
file_info_ready_callback (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
    NautilusImage *self = user_data;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInfo) info = g_file_query_info_finish (G_FILE (source_object), res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        /* The operation was cancelled, bailout. */
        return;
    }

    self->is_loading_attributes = FALSE;
    self->error = g_steal_pointer (&error);

    if (info == NULL || self->error != NULL)
    {
        handle_loading_error (self);

        return;
    }

    ThumbnailCacheItem *cache_item = thumbnail_cache_get (self->source);

    self->source_mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    self->source_content_type = g_strdup (g_file_info_get_content_type (info));

    /* Look in nautilus's thumbnail cache */
    if (cache_item != NULL &&
        cache_item->mtime == self->source_mtime)
    {
        nautilus_image_set_texture (self, cache_item->texture);

        return;
    }

    /* Look in the user's thumbnail cache */
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID) &&
        g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID))
    {
        const char *thumb_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

        if (thumb_path != NULL)
        {
            g_autoptr (GFile) thumb_file = g_file_new_for_path (thumb_path);

            g_file_read_async (thumb_file,
                               G_PRIORITY_DEFAULT,
                               self->cancellable,
                               thumbnail_file_read_callback,
                               self);

            return;
        }
    }

    guint64 file_size = g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE)
                        ? g_file_info_get_size (info)
                        : 0;

    if (file_size > cached_thumbnail_size_limit &&
        nautilus_thumbnail_is_mimetype_limited_by_size (self->source_content_type))
    {
        g_set_error (&self->error,
                     G_IO_ERROR,
                     G_IO_ERROR_FAILED,
                     "Image file is too large: %" G_GUINT64_FORMAT " bytes (max: %" G_GUINT64_FORMAT " bytes)",
                     file_size, cached_thumbnail_size_limit);
        handle_loading_error (self);

        return;
    }

    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ) &&
        !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        g_set_error (&self->error,
                     G_IO_ERROR,
                     G_IO_ERROR_PERMISSION_DENIED,
                     "No read permission for file");
        handle_loading_error (self);

        return;
    }

    /* Create a thumbnail */
    g_autofree gchar *uri = g_file_get_uri (self->source);

    if (!nautilus_can_thumbnail (uri,
                                 self->source_content_type,
                                 self->source_mtime))
    {
        g_set_error (&self->error,
                     G_IO_ERROR,
                     G_IO_ERROR_NOT_SUPPORTED,
                     "MIME type not supported for thumbnails: %s",
                     self->source_content_type ? self->source_content_type : "(unknown)");
        handle_loading_error (self);

        return;
    }

    nautilus_create_thumbnail_async (uri,
                                     self->source_content_type,
                                     self->source_mtime,
                                     self->cancellable,
                                     thumbnailing_done_cb,
                                     self);
}

static void
nautilus_image_set_texture (NautilusImage *self,
                            GdkTexture    *texture)
{
    gboolean resize = (self->texture == NULL && texture != NULL) ||
                      (self->texture != NULL && texture == NULL);

    if (self->texture != NULL && texture != NULL)
    {
        resize = (gdk_texture_get_width (self->texture) != gdk_texture_get_width (texture)) ||
                 (gdk_texture_get_height (self->texture) != gdk_texture_get_height (texture));
    }

    if (g_set_object (&self->texture, texture))
    {
        if (resize)
        {
            gtk_widget_queue_resize (GTK_WIDGET (self));
        }
        else
        {
            gtk_widget_queue_draw (GTK_WIDGET (self));
        }

        if (texture != NULL)
        {
            gtk_widget_add_css_class (GTK_WIDGET (self), "thumbnail");
        }
        else
        {
            gtk_widget_remove_css_class (GTK_WIDGET (self), "thumbnail");
        }
    }
}

void
nautilus_image_set_fallback (NautilusImage *self,
                             GdkPaintable  *paintable)
{
    if (g_set_object (&self->fallback_paintable, paintable))
    {
        gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

/**
 * nautilus_image_set_source:
 * @self: A #NautilusImage
 * @source: (nullable): A #GFile to set as the image source
 *
 * Sets the source file for the image to be loaded and displayed.
 *
 * If @source is the same as the currently set source file, this function
 * will return early without performing any checks to determine if the file
 * has changed. This optimization avoids unnecessary file queries when the
 * source hasn't changed.
 *
 * When a new source is set, the function will:
 * - Clear any previously loaded texture
 * - Cancel any pending file operations from previous source loads
 * - Query file information including size, content type, and modification time
 * - Check if a valid thumbnail exists in cache, otherwise create a new one
 *
 */
void
nautilus_image_set_source (NautilusImage *self,
                           GFile         *source)
{
    if (self->source == source ||
        (self->source != NULL && source != NULL &&
         g_file_equal (self->source, source)))
    {
        return;
    }

    g_set_object (&self->source, source);
    nautilus_image_set_texture (self, NULL);
    g_clear_pointer (&self->source_content_type, g_free);
    self->source_mtime = 0;
    g_clear_error (&self->error);

    self->is_loading_attributes = FALSE;
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);

    if (source != NULL)
    {
        self->cancellable = g_cancellable_new ();
        self->is_loading_attributes = TRUE;

        /* First query the file info to check size and type */
        g_file_query_info_async (self->source,
                                 G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                 G_FILE_ATTRIBUTE_ACCESS_CAN_READ ","
                                 G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                                 G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID ","
                                 G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                                 G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 self->cancellable,
                                 file_info_ready_callback,
                                 self);
    }

    gtk_widget_queue_draw (GTK_WIDGET (self));

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SOURCE]);
}

void
nautilus_image_set_size (NautilusImage *self,
                         gint           size)
{
    if (self->size != size)
    {
        self->size = size;

        gtk_widget_queue_resize (GTK_WIDGET (self));

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIZE]);
    }
}

static void
nautilus_image_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    NautilusImage *self = NAUTILUS_IMAGE (object);

    switch (property_id)
    {
        case PROP_SOURCE:
        {
            g_value_set_object (value, self->source);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_image_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    NautilusImage *self = NAUTILUS_IMAGE (object);

    switch (property_id)
    {
        case PROP_SOURCE:
        {
            nautilus_image_set_source (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_image_init (NautilusImage *self)
{
    gtk_widget_set_name (GTK_WIDGET (self), "NautilusImage");
}

static void
real_snapshot (GtkWidget   *widget,
               GtkSnapshot *snapshot)
{
    NautilusImage *self = NAUTILUS_IMAGE (widget);

    if (self->texture != NULL)
    {
        double width = gdk_texture_get_width (self->texture);
        double height = gdk_texture_get_height (self->texture);
        GskRoundedRect rounded_rect;
        const float border_radius = 2.0;

        if (MAX (width, height) != self->size)
        {
            float scale_factor = MAX (width, height) / self->size;

            width = width / scale_factor;
            height = height / scale_factor;
        }

        gsk_rounded_rect_init_from_rect (&rounded_rect,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                                         border_radius);
        gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);

        gdk_paintable_snapshot (GDK_PAINTABLE (self->texture),
                                GDK_SNAPSHOT (snapshot),
                                width, height);

        if (self->size >= NAUTILUS_GRID_ICON_SIZE_SMALL &&
            nautilus_mime_is_video (self->source_content_type))
        {
            nautilus_ui_frame_video (snapshot, width, height);
        }

        /* End rounded clip */
        gtk_snapshot_pop (snapshot);
    }
    else if (!self->is_loading_attributes &&
             self->cancellable != NULL &&
             self->error == NULL)
    {
        gdk_paintable_snapshot (get_loading_paintable (self),
                                GDK_SNAPSHOT (snapshot),
                                self->size, self->size);
    }
    else
    {
        GdkPaintable *paintable = get_error_paintable (self);

        if (self->fallback_paintable != NULL)
        {
            paintable = self->fallback_paintable;
        }

        gdk_paintable_snapshot (paintable,
                                GDK_SNAPSHOT (snapshot),
                                self->size, self->size);
    }

    GTK_WIDGET_CLASS (nautilus_image_parent_class)->snapshot (widget, snapshot);
}

static void
real_measure (GtkWidget      *widget,
              GtkOrientation  orientation,
              int             for_size,
              int            *minimum,
              int            *natural,
              int            *minimum_baseline,
              int            *natural_baseline)
{
    NautilusImage *self = NAUTILUS_IMAGE (widget);
    int length = self->size;

    if (self->texture != NULL)
    {
        double width = gdk_texture_get_width (self->texture);
        double height = gdk_texture_get_height (self->texture);

        if (MAX (width, height) != self->size)
        {
            float scale_factor = MAX (width, height) / self->size;

            width = width / scale_factor;
            height = height / scale_factor;
        }

        length = orientation == GTK_ORIENTATION_HORIZONTAL ? width : height;
    }

    *minimum = length;
    *natural = length;
    *minimum_baseline = -1;
    *natural_baseline = -1;
}

static void
nautilus_image_dispose (GObject *object)
{
    NautilusImage *self = NAUTILUS_IMAGE (object);

    g_clear_object (&self->source);
    g_clear_object (&self->texture);
    g_clear_pointer (&self->source_content_type, g_free);
    g_clear_object (&self->fallback_paintable);
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_error (&self->error);

    G_OBJECT_CLASS (nautilus_image_parent_class)->dispose (object);
}

static void
nautilus_image_class_init (NautilusImageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->get_property = nautilus_image_get_property;
    object_class->set_property = nautilus_image_set_property;

    object_class->dispose = nautilus_image_dispose;

    widget_class->snapshot = real_snapshot;
    widget_class->measure = real_measure;

    update_thumbnail_limit ();
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT,
                              G_CALLBACK (update_thumbnail_limit),
                              NULL);

    properties[PROP_SOURCE] = g_param_spec_object ("source", NULL, NULL,
                                                   G_TYPE_FILE,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_STATIC_STRINGS);

    properties[PROP_SIZE] = g_param_spec_int ("size", NULL, NULL,
                                              0, 1024, 0,
                                              G_PARAM_READWRITE |
                                              G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

NautilusImage *
nautilus_image_new (void)
{
    return NAUTILUS_IMAGE (g_object_new (NAUTILUS_TYPE_IMAGE, NULL));
}
