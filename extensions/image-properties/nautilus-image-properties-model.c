/* Copyright (C) 2004 Red Hat, Inc
 * Copyright (c) 2007 Novell, Inc.
 * Copyright (c) 2017 Thomas Bechtold <thomasbechtold@jpberlin.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 * XMP support by Hubert Figuiere <hfiguiere@novell.com>
 */

#include "nautilus-image-properties-model.h"

#include <gexiv2/gexiv2.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <math.h>

#define LOAD_BUFFER_SIZE 8192

typedef struct
{
    GListStore *group_model;

    GCancellable *cancellable;
    GdkPixbufLoader *loader;
    gboolean got_size;
    gboolean pixbuf_still_loading;
    unsigned char buffer[LOAD_BUFFER_SIZE];
    int width;
    int height;

    GExiv2Metadata *md;
    gboolean md_ready;
} NautilusImagesPropertiesModel;

static void
nautilus_images_properties_model_free (NautilusImagesPropertiesModel *self)
{
    if (self->cancellable != NULL)
    {
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
    }
    g_free (self);
}

static void
append_item (NautilusImagesPropertiesModel *self,
             const char                    *name,
             const char                    *value)
{
    g_autoptr (NautilusPropertiesItem) item = NULL;

    item = nautilus_properties_item_new (name, value);
    g_list_store_append (self->group_model, item);
}

static void
nautilus_image_properties_model_init (NautilusImagesPropertiesModel *self)
{
    self->group_model = g_list_store_new (NAUTILUS_TYPE_PROPERTIES_ITEM);
}

static void
append_basic_info (NautilusImagesPropertiesModel *self)
{
    GdkPixbufFormat *format;
    GExiv2Orientation orientation = GEXIV2_ORIENTATION_UNSPECIFIED;
    int width;
    int height;
    g_autofree char *name = NULL;
    g_autofree char *desc = NULL;
    g_autofree char *value = NULL;

    format = gdk_pixbuf_loader_get_format (self->loader);
    name = gdk_pixbuf_format_get_name (format);
    desc = gdk_pixbuf_format_get_description (format);
    value = g_strdup_printf ("%s (%s)", name, desc);

    append_item (self, _("Image Type"), value);

    if (self->md_ready)
    {
        orientation = gexiv2_metadata_try_get_orientation (self->md, NULL);
    }

    if (orientation == GEXIV2_ORIENTATION_ROT_90
        || orientation == GEXIV2_ORIENTATION_ROT_270
        || orientation == GEXIV2_ORIENTATION_ROT_90_HFLIP
        || orientation == GEXIV2_ORIENTATION_ROT_90_VFLIP)
    {
        width = self->height;
        height = self->width;
    }
    else
    {
        width = self->width;
        height = self->height;
    }

    g_free (value);
    value = g_strdup_printf (ngettext ("%d pixel",
                                       "%d pixels",
                                       width),
                             width);

    append_item (self, _("Width"), value);

    g_free (value);
    value = g_strdup_printf (ngettext ("%d pixel",
                                       "%d pixels",
                                       height),
                             height);

    append_item (self, _("Height"), value);
}

static void
append_gexiv2_tag (NautilusImagesPropertiesModel  *self,
                   const char                    **tag_names,
                   const char                     *description)
{
    g_assert (tag_names != NULL);

    for (const char **i = tag_names; *i != NULL; i++)
    {
        if (gexiv2_metadata_try_has_tag (self->md, *i, NULL))
        {
            g_autofree char *tag_value = NULL;

            tag_value = gexiv2_metadata_try_get_tag_interpreted_string (self->md, *i, NULL);

            if (description == NULL)
            {
                description = gexiv2_metadata_try_get_tag_description (*i, NULL);
            }

            /* don't add empty tags - try next one */
            if (strlen (tag_value) > 0)
            {
                append_item (self, description, tag_value);
                break;
            }
        }
    }
}

static void
append_gexiv2_info (NautilusImagesPropertiesModel *self)
{
    double longitude;
    double latitude;
    double altitude;

    /* define tags and its alternatives */
    const char *title[] = { "Xmp.dc.title", NULL };
    const char *camera_brand[] = { "Exif.Image.Make", NULL };
    const char *camera_model[] = { "Exif.Image.Model", "Exif.Image.UniqueCameraModel", NULL };
    const char *created_on[] = { "Exif.Photo.DateTimeOriginal", "Xmp.xmp.CreateDate", "Exif.Image.DateTime", NULL };
    const char *exposure_time[] = { "Exif.Photo.ExposureTime", NULL };
    const char *aperture_value[] = { "Exif.Photo.ApertureValue", NULL };
    const char *iso_speed_ratings[] = { "Exif.Photo.ISOSpeedRatings", "Xmp.exifEX.ISOSpeed", NULL };
    const char *flash[] = { "Exif.Photo.Flash", NULL };
    const char *metering_mode[] = { "Exif.Photo.MeteringMode", NULL };
    const char *exposure_mode[] = { "Exif.Photo.ExposureMode", NULL };
    const char *focal_length[] = { "Exif.Photo.FocalLength", NULL };
    const char *software[] = { "Exif.Image.Software", NULL };
    const char *description[] = { "Xmp.dc.description", "Exif.Photo.UserComment", NULL };
    const char *subject[] = { "Xmp.dc.subject", NULL };
    const char *creator[] = { "Xmp.dc.creator", "Exif.Image.Artist", NULL };
    const char *rights[] = { "Xmp.dc.rights", NULL };
    const char *rating[] = { "Xmp.xmp.Rating", NULL };

    if (!self->md_ready)
    {
        return;
    }

    append_gexiv2_tag (self, camera_brand, _("Camera Brand"));
    append_gexiv2_tag (self, camera_model, _("Camera Model"));
    append_gexiv2_tag (self, exposure_time, _("Exposure Time"));
    append_gexiv2_tag (self, exposure_mode, _("Exposure Program"));
    append_gexiv2_tag (self, aperture_value, _("Aperture Value"));
    append_gexiv2_tag (self, iso_speed_ratings, _("ISO Speed Rating"));
    append_gexiv2_tag (self, flash, _("Flash Fired"));
    append_gexiv2_tag (self, metering_mode, _("Metering Mode"));
    append_gexiv2_tag (self, focal_length, _("Focal Length"));
    append_gexiv2_tag (self, software, _("Software"));
    append_gexiv2_tag (self, title, _("Title"));
    append_gexiv2_tag (self, description, _("Description"));
    append_gexiv2_tag (self, subject, _("Keywords"));
    append_gexiv2_tag (self, creator, _("Creator"));
    append_gexiv2_tag (self, created_on, _("Created On"));
    append_gexiv2_tag (self, rights, _("Copyright"));
    append_gexiv2_tag (self, rating, _("Rating"));

    if (gexiv2_metadata_try_get_gps_info (self->md, &longitude, &latitude, &altitude, NULL))
    {
        g_autofree char *gps_coords = NULL;

        gps_coords = g_strdup_printf ("%f° %s %f° %s (%.0f m)",
                                      fabs (latitude),
                                      /* Translators: "N" and "S" stand for
                                       * north and south in GPS coordinates. */
                                      latitude >= 0 ? _("N") : _("S"),
                                      fabs (longitude),
                                      /* Translators: "E" and "W" stand for
                                       * east and west in GPS coordinates. */
                                      longitude >= 0 ? _("E") : _("W"),
                                      altitude);

        append_item (self, _("Coordinates"), gps_coords);
    }
}

static void
load_finished (NautilusImagesPropertiesModel *self)
{
    if (self->loader != NULL)
    {
        gdk_pixbuf_loader_close (self->loader, NULL);
    }

    if (self->got_size)
    {
        append_basic_info (self);
        append_gexiv2_info (self);
    }
    else
    {
        append_item (self, _("Oops! Something went wrong."), _("Failed to load image information"));
    }

    if (self->loader != NULL)
    {
        g_object_unref (self->loader);
        self->loader = NULL;
    }
    self->md_ready = FALSE;
    g_clear_object (&self->md);
}

static void
file_close_callback (GObject      *object,
                     GAsyncResult *res,
                     gpointer      data)
{
    NautilusImagesPropertiesModel *self;
    GInputStream *stream;

    self = data;
    stream = G_INPUT_STREAM (object);

    g_input_stream_close_finish (stream, res, NULL);

    g_clear_object (&self->cancellable);
}

static void
file_read_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      data)
{
    NautilusImagesPropertiesModel *self;
    GInputStream *stream;
    g_autoptr (GError) error = NULL;
    gssize count_read;
    gboolean done_reading;

    self = data;
    stream = G_INPUT_STREAM (object);
    count_read = g_input_stream_read_finish (stream, res, &error);
    done_reading = FALSE;

    if (count_read > 0)
    {
        g_assert (count_read <= sizeof (self->buffer));

        if (self->pixbuf_still_loading)
        {
            if (!gdk_pixbuf_loader_write (self->loader,
                                          self->buffer,
                                          count_read,
                                          NULL))
            {
                self->pixbuf_still_loading = FALSE;
            }
        }

        if (self->pixbuf_still_loading)
        {
            g_input_stream_read_async (G_INPUT_STREAM (stream),
                                       self->buffer,
                                       sizeof (self->buffer),
                                       G_PRIORITY_DEFAULT,
                                       self->cancellable,
                                       file_read_callback,
                                       self);
        }
        else
        {
            done_reading = TRUE;
        }
    }
    else
    {
        /* either EOF, cancelled or an error occurred */
        done_reading = TRUE;
    }

    if (error != NULL)
    {
        g_autofree char *uri = NULL;

        uri = g_file_get_uri (G_FILE (object));

        g_warning ("Error reading %s: %s", uri, error->message);
    }

    if (done_reading)
    {
        load_finished (self);
        g_input_stream_close_async (stream,
                                    G_PRIORITY_DEFAULT,
                                    self->cancellable,
                                    file_close_callback,
                                    self);
    }
}

static void
size_prepared_callback (GdkPixbufLoader *loader,
                        int              width,
                        int              height,
                        gpointer         callback_data)
{
    NautilusImagesPropertiesModel *self;

    self = callback_data;

    self->height = height;
    self->width = width;
    self->got_size = TRUE;
    self->pixbuf_still_loading = FALSE;
}

typedef struct
{
    NautilusImagesPropertiesModel *self;
    NautilusFileInfo *file_info;
} FileOpenData;

static void
file_open_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    g_autofree FileOpenData *data = NULL;
    NautilusImagesPropertiesModel *self;
    GFile *file;
    g_autofree char *uri = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInputStream) stream = NULL;

    data = user_data;
    self = data->self;
    file = G_FILE (object);
    uri = g_file_get_uri (file);
    stream = g_file_read_finish (file, res, &error);
    if (stream != NULL)
    {
        g_autofree char *mime_type = NULL;

        mime_type = nautilus_file_info_get_mime_type (data->file_info);

        self->loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, &error);
        if (error != NULL)
        {
            g_warning ("Error creating loader for %s: %s", uri, error->message);
        }
        self->pixbuf_still_loading = TRUE;
        self->width = 0;
        self->height = 0;

        g_signal_connect (self->loader,
                          "size-prepared",
                          G_CALLBACK (size_prepared_callback),
                          self);

        g_input_stream_read_async (G_INPUT_STREAM (stream),
                                   self->buffer,
                                   sizeof (self->buffer),
                                   G_PRIORITY_DEFAULT,
                                   self->cancellable,
                                   file_read_callback,
                                   self);
    }
    else
    {
        g_warning ("Error reading %s: %s", uri, error->message);
        load_finished (self);
    }
}

static void
nautilus_image_properties_model_load_from_file_info (NautilusImagesPropertiesModel *self,
                                                     NautilusFileInfo              *file_info)
{
    g_autofree char *uri = NULL;
    g_autoptr (GFile) file = NULL;
    g_autofree char *path = NULL;
    FileOpenData *data;

    g_return_if_fail (file_info != NULL);

    self->cancellable = g_cancellable_new ();

    uri = nautilus_file_info_get_uri (file_info);
    file = g_file_new_for_uri (uri);
    path = g_file_get_path (file);

    /* gexiv2 metadata init */
    self->md_ready = gexiv2_initialize ();
    if (!self->md_ready)
    {
        g_warning ("Unable to initialize gexiv2");
    }
    else
    {
        self->md = gexiv2_metadata_new ();
        if (path != NULL)
        {
            g_autoptr (GError) error = NULL;

            if (!gexiv2_metadata_open_path (self->md, path, &error))
            {
                g_warning ("gexiv2 metadata not supported for '%s': %s", path, error->message);
                self->md_ready = FALSE;
            }
        }
        else
        {
            self->md_ready = FALSE;
        }
    }

    data = g_new0 (FileOpenData, 1);

    data->self = self;
    data->file_info = file_info;

    g_file_read_async (file,
                       G_PRIORITY_DEFAULT,
                       self->cancellable,
                       file_open_callback,
                       data);
}

NautilusPropertiesModel *
nautilus_image_properties_model_new (NautilusFileInfo *file_info)
{
    NautilusImagesPropertiesModel *self;
    NautilusPropertiesModel *model;

    self = g_new0 (NautilusImagesPropertiesModel, 1);

    nautilus_image_properties_model_init (self);
    nautilus_image_properties_model_load_from_file_info (self, file_info);

    model = nautilus_properties_model_new (_("Image Properties"),
                                           G_LIST_MODEL (self->group_model));

    g_object_weak_ref (G_OBJECT (model),
                       (GWeakNotify) nautilus_images_properties_model_free,
                       self);

    return model;
}
