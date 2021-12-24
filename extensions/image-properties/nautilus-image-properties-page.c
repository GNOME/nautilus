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

#include "nautilus-image-properties-page.h"

#include <gexiv2/gexiv2.h>
#include <glib/gi18n.h>

#include <math.h>

#define LOAD_BUFFER_SIZE 8192

typedef struct
{
    GtkWidget *page_widget;

    GCancellable *cancellable;
    GtkWidget *grid;
    GdkPixbufLoader *loader;
    gboolean got_size;
    gboolean pixbuf_still_loading;
    unsigned char buffer[LOAD_BUFFER_SIZE];
    int width;
    int height;

    GExiv2Metadata *md;
    gboolean md_ready;
} NautilusImagesPropertiesPage;

static void
nautilus_images_properties_page_free (NautilusImagesPropertiesPage *page)
{
    if (page->cancellable != NULL)
    {
        g_cancellable_cancel (page->cancellable);
        g_clear_object (&page->cancellable);
    }
    g_free (page);
}

static void
append_item (NautilusImagesPropertiesPage *page,
             const char                   *name,
             const char                   *value)
{
    GtkWidget *name_label;
    PangoAttrList *attrs;

    name_label = gtk_label_new (name);
    attrs = pango_attr_list_new ();

    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes (GTK_LABEL (name_label), attrs);
    pango_attr_list_unref (attrs);
    gtk_grid_attach_next_to (GTK_GRID (page->grid), name_label, NULL, GTK_POS_BOTTOM, 1, 1);
    gtk_widget_set_halign (name_label, GTK_ALIGN_START);
    gtk_widget_show (name_label);

    if (value != NULL)
    {
        GtkWidget *value_label;

        value_label = gtk_label_new (value);

        gtk_label_set_line_wrap (GTK_LABEL (value_label), TRUE);
        gtk_grid_attach_next_to (GTK_GRID (page->grid), value_label,
                                 name_label, GTK_POS_RIGHT,
                                 1, 1);
        gtk_widget_set_halign (value_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand (value_label, TRUE);
        gtk_widget_show (value_label);
    }
}

static void
nautilus_image_properties_page_init (NautilusImagesPropertiesPage *self)
{
    self->page_widget = gtk_scrolled_window_new (NULL, NULL);

    g_object_set (self->page_widget,
                  "margin-bottom", 6,
                  "margin-end", 12,
                  "margin-start", 12,
                  "margin-top", 6,
                  NULL);
    gtk_widget_set_vexpand (self->page_widget, TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->page_widget),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);

    self->grid = gtk_grid_new ();

    gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
    gtk_grid_set_row_spacing (GTK_GRID (self->grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (self->grid), 18);
    append_item (self, _("Loading…"), NULL);
#if GTK_MAJOR_VERSION < 4
    gtk_container_add (GTK_CONTAINER (self->page_widget), self->grid);
#else
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->page_widget),
                                   self->grid);
#endif

    gtk_widget_show (GTK_WIDGET (self->page_widget));
}

static void
append_basic_info (NautilusImagesPropertiesPage *page)
{
    GdkPixbufFormat *format;
    GExiv2Orientation orientation;
    int width;
    int height;
    g_autofree char *name = NULL;
    g_autofree char *desc = NULL;
    g_autofree char *value = NULL;

    format = gdk_pixbuf_loader_get_format (page->loader);
    name = gdk_pixbuf_format_get_name (format);
    desc = gdk_pixbuf_format_get_description (format);
    value = g_strdup_printf ("%s (%s)", name, desc);

    append_item (page, _("Image Type"), value);

    orientation = gexiv2_metadata_try_get_orientation (page->md, NULL);

    if (orientation == GEXIV2_ORIENTATION_ROT_90
        || orientation == GEXIV2_ORIENTATION_ROT_270
        || orientation == GEXIV2_ORIENTATION_ROT_90_HFLIP
        || orientation == GEXIV2_ORIENTATION_ROT_90_VFLIP)
    {
        width = page->height;
        height = page->width;
    }
    else
    {
        width = page->width;
        height = page->height;
    }

    g_free (value);
    value = g_strdup_printf (ngettext ("%d pixel",
                                       "%d pixels",
                                       width),
                             width);

    append_item (page, _("Width"), value);

    g_free (value);
    value = g_strdup_printf (ngettext ("%d pixel",
                                       "%d pixels",
                                       height),
                             height);

    append_item (page, _("Height"), value);
}

static void
append_gexiv2_tag (NautilusImagesPropertiesPage  *page,
                   const char                   **tag_names,
                   const char                    *description)
{
    g_assert (tag_names != NULL);

    for (const char **i = tag_names; *i != NULL; i++)
    {
        if (gexiv2_metadata_try_has_tag (page->md, *i, NULL))
        {
            g_autofree char *tag_value = NULL;

            tag_value = gexiv2_metadata_try_get_tag_interpreted_string (page->md, *i, NULL);

            if (description == NULL)
            {
                description = gexiv2_metadata_try_get_tag_description (*i, NULL);
            }

            /* don't add empty tags - try next one */
            if (strlen (tag_value) > 0)
            {
                append_item (page, description, tag_value);
                break;
            }
        }
    }
}

static void
append_gexiv2_info (NautilusImagesPropertiesPage *page)
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

    if (!page->md_ready)
    {
        return;
    }

    append_gexiv2_tag (page, camera_brand, _("Camera Brand"));
    append_gexiv2_tag (page, camera_model, _("Camera Model"));
    append_gexiv2_tag (page, exposure_time, _("Exposure Time"));
    append_gexiv2_tag (page, exposure_mode, _("Exposure Program"));
    append_gexiv2_tag (page, aperture_value, _("Aperture Value"));
    append_gexiv2_tag (page, iso_speed_ratings, _("ISO Speed Rating"));
    append_gexiv2_tag (page, flash, _("Flash Fired"));
    append_gexiv2_tag (page, metering_mode, _("Metering Mode"));
    append_gexiv2_tag (page, focal_length, _("Focal Length"));
    append_gexiv2_tag (page, software, _("Software"));
    append_gexiv2_tag (page, title, _("Title"));
    append_gexiv2_tag (page, description, _("Description"));
    append_gexiv2_tag (page, subject, _("Keywords"));
    append_gexiv2_tag (page, creator, _("Creator"));
    append_gexiv2_tag (page, created_on, _("Created On"));
    append_gexiv2_tag (page, rights, _("Copyright"));
    append_gexiv2_tag (page, rating, _("Rating"));

    if (gexiv2_metadata_try_get_gps_info (page->md, &longitude, &latitude, &altitude, NULL))
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

        append_item (page, _("Coordinates"), gps_coords);
    }
}

static void
load_finished (NautilusImagesPropertiesPage *page)
{
    GtkWidget *label;

    label = gtk_grid_get_child_at (GTK_GRID (page->grid), 0, 0);
    gtk_widget_hide (label);

    if (page->loader != NULL)
    {
        gdk_pixbuf_loader_close (page->loader, NULL);
    }

    if (page->got_size)
    {
        append_basic_info (page);
        append_gexiv2_info (page);
    }
    else
    {
        append_item (page, _("Failed to load image information"), NULL);
    }

    if (page->loader != NULL)
    {
        g_object_unref (page->loader);
        page->loader = NULL;
    }
    page->md_ready = FALSE;
    g_clear_object (&page->md);
}

static void
file_close_callback (GObject      *object,
                     GAsyncResult *res,
                     gpointer      data)
{
    NautilusImagesPropertiesPage *page;
    GInputStream *stream;

    page = data;
    stream = G_INPUT_STREAM (object);

    g_input_stream_close_finish (stream, res, NULL);

    g_clear_object (&page->cancellable);
}

static void
file_read_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      data)
{
    NautilusImagesPropertiesPage *page;
    GInputStream *stream;
    g_autoptr (GError) error = NULL;
    gssize count_read;
    gboolean done_reading;

    page = data;
    stream = G_INPUT_STREAM (object);
    count_read = g_input_stream_read_finish (stream, res, &error);
    done_reading = FALSE;

    if (count_read > 0)
    {
        g_assert (count_read <= sizeof (page->buffer));

        if (page->pixbuf_still_loading)
        {
            if (!gdk_pixbuf_loader_write (page->loader,
                                          page->buffer,
                                          count_read,
                                          NULL))
            {
                page->pixbuf_still_loading = FALSE;
            }
        }

        if (page->pixbuf_still_loading)
        {
            g_input_stream_read_async (G_INPUT_STREAM (stream),
                                       page->buffer,
                                       sizeof (page->buffer),
                                       G_PRIORITY_DEFAULT,
                                       page->cancellable,
                                       file_read_callback,
                                       page);
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
        load_finished (page);
        g_input_stream_close_async (stream,
                                    G_PRIORITY_DEFAULT,
                                    page->cancellable,
                                    file_close_callback,
                                    page);
    }
}

static void
size_prepared_callback (GdkPixbufLoader *loader,
                        int              width,
                        int              height,
                        gpointer         callback_data)
{
    NautilusImagesPropertiesPage *page;

    page = callback_data;

    page->height = height;
    page->width = width;
    page->got_size = TRUE;
    page->pixbuf_still_loading = FALSE;
}

typedef struct
{
    NautilusImagesPropertiesPage *page;
    NautilusFileInfo *file_info;
} FileOpenData;

static void
file_open_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    g_autofree FileOpenData *data = NULL;
    NautilusImagesPropertiesPage *page;
    GFile *file;
    g_autofree char *uri = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFileInputStream) stream = NULL;

    data = user_data;
    page = data->page;
    file = G_FILE (object);
    uri = g_file_get_uri (file);
    stream = g_file_read_finish (file, res, &error);
    if (stream != NULL)
    {
        g_autofree char *mime_type = NULL;

        mime_type = nautilus_file_info_get_mime_type (data->file_info);

        page->loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, &error);
        if (error != NULL)
        {
            g_warning ("Error creating loader for %s: %s", uri, error->message);
        }
        page->pixbuf_still_loading = TRUE;
        page->width = 0;
        page->height = 0;

        g_signal_connect (page->loader,
                          "size-prepared",
                          G_CALLBACK (size_prepared_callback),
                          page);

        g_input_stream_read_async (G_INPUT_STREAM (stream),
                                   page->buffer,
                                   sizeof (page->buffer),
                                   G_PRIORITY_DEFAULT,
                                   page->cancellable,
                                   file_read_callback,
                                   page);
    }
    else
    {
        g_warning ("Error reading %s: %s", uri, error->message);
        load_finished (page);
    }
}

static void
nautilus_image_properties_page_load_from_file_info (NautilusImagesPropertiesPage *self,
                                                    NautilusFileInfo             *file_info)
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

    data->page = self;
    data->file_info = file_info;

    g_file_read_async (file,
                       G_PRIORITY_DEFAULT,
                       self->cancellable,
                       file_open_callback,
                       data);
}

GtkWidget *
nautilus_image_properties_page_new (NautilusFileInfo *file_info)
{
    NautilusImagesPropertiesPage *self;

    self = g_new0 (NautilusImagesPropertiesPage, 1);

    nautilus_image_properties_page_init (self);
    nautilus_image_properties_page_load_from_file_info (self, file_info);

    g_object_set_data_full (G_OBJECT (self->page_widget),
                            "nautilus-images-properties-page",
                            self,
                            (GDestroyNotify) nautilus_images_properties_page_free);

    return self->page_widget;
}
