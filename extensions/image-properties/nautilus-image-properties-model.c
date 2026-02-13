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

#include <math.h>
#include <stdio.h>

typedef struct
{
    GListStore *group_model;

    GExiv2Metadata *md;
} NautilusImagesPropertiesModel;

/* tags and their alternatives */
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

static void
nautilus_images_properties_model_free (NautilusImagesPropertiesModel *self)
{
    g_clear_object (&self->md);
    g_clear_object (&self->group_model);

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
    const char *mime_type = gexiv2_metadata_get_mime_type (self->md);
    g_autofree char *desc = g_content_type_get_description (mime_type);
    g_autofree char *value = (desc != NULL)
                             ? g_strdup_printf ("%s (%s)", desc, mime_type)
                             : g_strdup (mime_type);

    append_item (self, _("Image Type"), value);

    GExiv2Orientation orientation = gexiv2_metadata_get_orientation (self->md, NULL);
    int width = gexiv2_metadata_get_pixel_width (self->md);
    int height = gexiv2_metadata_get_pixel_height (self->md);

    if (orientation == GEXIV2_ORIENTATION_ROT_90
        || orientation == GEXIV2_ORIENTATION_ROT_270
        || orientation == GEXIV2_ORIENTATION_ROT_90_HFLIP
        || orientation == GEXIV2_ORIENTATION_ROT_90_VFLIP)
    {
        /* Swap height and width due to orientation */
        int swap = width;

        width = height;
        height = swap;
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
format_exif_datetime (gchar **tag_value)
{
    gint year, month, day, hour, minute, seconds, count;

    count = sscanf (*tag_value, "%d:%d:%d %d:%d:%d",
                    &year, &month, &day, &hour, &minute, &seconds);

    if (count == 6)
    {
        g_autoptr (GDateTime) datetime = g_date_time_new_utc (year, month, day,
                                                              hour, minute, seconds);

        if (datetime != NULL)
        {
            g_free (*tag_value);
            /* TODO: Use the date format from Nautilus */
            *tag_value = g_date_time_format (datetime, "%F %T");
        }
    }
}

static void
append_gexiv2_tag (NautilusImagesPropertiesModel  *self,
                   const char                    **tag_names,
                   const char                     *tag_description)
{
    g_assert (tag_names != NULL);

    for (const char **i = tag_names; *i != NULL; i++)
    {
        if (gexiv2_metadata_has_tag (self->md, *i, NULL))
        {
            g_autofree char *tag_value = gexiv2_metadata_get_tag_interpreted_string (self->md,
                                                                                     *i,
                                                                                     NULL);

            if (tag_description == NULL)
            {
                tag_description = gexiv2_metadata_get_tag_description (*i, NULL);
            }

            /* don't add empty tags - try next one */
            if (tag_value != NULL && strlen (tag_value) > 0)
            {
                if (tag_names == created_on)
                {
                    format_exif_datetime (&tag_value);
                }

                append_item (self, tag_description, tag_value);
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

    gexiv2_metadata_get_gps_info (self->md, &longitude, &latitude, &altitude, NULL);

    if (isnan (longitude) == 0 && isinf (longitude) == 0 &&
        isnan (latitude) == 0 && isinf (latitude) == 0)
    {
        g_autoptr (GString) gps_coords = g_string_new ("");

        g_string_append_printf (gps_coords, "%f° %s %f° %s",
                                latitude,
                                /* Translators: "N" and "S" stand for
                                 * north and south in GPS coordinates. */
                                latitude >= 0 ? _("N") : _("S"),
                                longitude,
                                /* Translators: "E" and "W" stand for
                                 * east and west in GPS coordinates. */
                                longitude >= 0 ? _("E") : _("W"));

        if (isnan (altitude) == 0 && isinf (altitude) == 0)
        {
            g_string_append_printf (gps_coords, " (%.0f m)", altitude);
        }

        append_item (self, _("Coordinates"), gps_coords->str);
    }
}

static void
nautilus_image_properties_model_load_from_file_info (NautilusImagesPropertiesModel *self,
                                                     NautilusFileInfo              *file_info)
{
    g_return_if_fail (file_info != NULL);

    g_autoptr (GError) error = NULL;
    g_autofree char *uri = nautilus_file_info_get_uri (file_info);
    g_autoptr (GFile) file = g_file_new_for_uri (uri);
    const char *path = g_file_peek_path (file);

    if (path == NULL)
    {
        /* Handle locations like recent:// */
        g_clear_pointer (&uri, g_free);
        g_clear_object (&file);

        uri = nautilus_file_info_get_activation_uri (file_info);
        file = g_file_new_for_uri (uri);
        path = g_file_peek_path (file);
    }

    g_return_if_fail (path != NULL);

    /* Image properties relies on gexiv2 metadata */
    if (!gexiv2_initialize ())
    {
        g_warning ("Unable to initialize gexiv2");

        return;
    }

    self->md = gexiv2_metadata_new ();

    if (gexiv2_metadata_open_path (self->md, path, &error))
    {
        append_basic_info (self);
        append_gexiv2_info (self);
    }
    else
    {
        g_warning ("gexiv2 metadata not supported for '%s': %s", path, error->message);
        append_item (self, _("Oops! Something went wrong."), _("Failed to load image information"));
    }
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
