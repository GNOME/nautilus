/*
 * Copyright (C) 2004 Red Hat, Inc
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

#include <config.h>
#include "nautilus-image-properties-page.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-vfs-extensions.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>
#include "nautilus-module.h"
#include <string.h>

#include <gexiv2/gexiv2.h>

#define LOAD_BUFFER_SIZE 8192

struct _NautilusImagePropertiesPage
{
    GtkBox parent;
    GCancellable *cancellable;
    GtkWidget *grid;
    GdkPixbufLoader *loader;
    gboolean got_size;
    gboolean pixbuf_still_loading;
    char buffer[LOAD_BUFFER_SIZE];
    int width;
    int height;

    GExiv2Metadata *md;
    gboolean md_ready;
};

enum
{
    PROP_URI
};

typedef struct
{
    GObject parent;
} NautilusImagePropertiesPageProvider;

typedef struct
{
    GObjectClass parent;
} NautilusImagePropertiesPageProviderClass;


static GType nautilus_image_properties_page_provider_get_type (void);
static void  property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface);


G_DEFINE_TYPE (NautilusImagePropertiesPage, nautilus_image_properties_page, GTK_TYPE_BOX);

G_DEFINE_TYPE_WITH_CODE (NautilusImagePropertiesPageProvider, nautilus_image_properties_page_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
                                                property_page_provider_iface_init));

static void
nautilus_image_properties_page_finalize (GObject *object)
{
    NautilusImagePropertiesPage *page;

    page = NAUTILUS_IMAGE_PROPERTIES_PAGE (object);

    if (page->cancellable)
    {
        g_cancellable_cancel (page->cancellable);
        g_object_unref (page->cancellable);
        page->cancellable = NULL;
    }

    G_OBJECT_CLASS (nautilus_image_properties_page_parent_class)->finalize (object);
}

static void
file_close_callback (GObject      *object,
                     GAsyncResult *res,
                     gpointer      data)
{
    NautilusImagePropertiesPage *page;
    GInputStream *stream;

    page = NAUTILUS_IMAGE_PROPERTIES_PAGE (data);
    stream = G_INPUT_STREAM (object);

    g_input_stream_close_finish (stream, res, NULL);

    g_object_unref (page->cancellable);
    page->cancellable = NULL;
}

static void
append_item (NautilusImagePropertiesPage *page,
             const char                  *name,
             const char                  *value)
{
    GtkWidget *name_label;
    GtkWidget *label;
    PangoAttrList *attrs;

    name_label = gtk_label_new (name);
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes (GTK_LABEL (name_label), attrs);
    pango_attr_list_unref (attrs);
    gtk_label_set_xalign (GTK_LABEL (name_label), 0);
    gtk_label_set_yalign (GTK_LABEL (name_label), 0);
    gtk_container_add (GTK_CONTAINER (page->grid), name_label);
    gtk_widget_show (name_label);

    if (value != NULL)
    {
        label = gtk_label_new (value);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_label_set_yalign (GTK_LABEL (label), 0);
        gtk_grid_attach_next_to (GTK_GRID (page->grid), label,
                                 name_label, GTK_POS_RIGHT,
                                 1, 1);
        gtk_widget_show (label);
    }
}

static void
append_basic_info (NautilusImagePropertiesPage *page)
{
    GdkPixbufFormat *format;
    char *name;
    char *desc;
    char *value;

    format = gdk_pixbuf_loader_get_format (page->loader);

    name = gdk_pixbuf_format_get_name (format);
    desc = gdk_pixbuf_format_get_description (format);
    value = g_strdup_printf ("%s (%s)", name, desc);
    g_free (name);
    g_free (desc);
    append_item (page, _("Image Type"), value);
    g_free (value);
    value = g_strdup_printf (ngettext ("%d pixel",
                                       "%d pixels",
                                       page->width),
                             page->width);
    append_item (page, _("Width"), value);
    g_free (value);
    value = g_strdup_printf (ngettext ("%d pixel",
                                       "%d pixels",
                                       page->height),
                             page->height);
    append_item (page, _("Height"), value);
    g_free (value);
}

static void
append_gexiv2_tag (NautilusImagePropertiesPage *page,
                   const gchar **tag_names,
                   gchar *description)
{
  gchar *tag_value;
  while (*tag_names)
    {
      if (gexiv2_metadata_has_tag (page->md, *tag_names))
        {
          tag_value = gexiv2_metadata_get_tag_interpreted_string (page->md, *tag_names);
          if (!description)
            description = gexiv2_metadata_get_tag_description (*tag_names);
          /* don't add empty tags - try next one */
          if (strlen (tag_value) > 0)
            {
              append_item (page, description, tag_value);
              g_free (tag_value);
              break;
            }
          g_free (tag_value);
        }
      *tag_names++;
    }
}

static void
append_gexiv2_info(NautilusImagePropertiesPage *page)
{
  gdouble longitude, latitude, altitude;
  gchar *gps_coords;

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

  if (gexiv2_metadata_get_gps_info (page->md, &longitude, &latitude, &altitude))
    {
      gps_coords = g_strdup_printf ("%f N / %f W (%.0f m)", latitude, longitude, altitude);
      append_item (page, _("Coordinates"), gps_coords);
      g_free (gps_coords);
    }
  /* TODO add CC licenses */
}

static void
load_finished (NautilusImagePropertiesPage *page)
{
    GtkWidget *label;

    label = gtk_grid_get_child_at (GTK_GRID (page->grid), 0, 0);
    gtk_container_remove (GTK_CONTAINER (page->grid), label);

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
file_read_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      data)
{
    NautilusImagePropertiesPage *page;
    GInputStream *stream;
    gssize count_read;
    GError *error;
    gboolean done_reading;

    page = NAUTILUS_IMAGE_PROPERTIES_PAGE (data);
    stream = G_INPUT_STREAM (object);

    error = NULL;
    done_reading = FALSE;
    count_read = g_input_stream_read_finish (stream, res, &error);

    if (count_read > 0)
    {
        g_assert (count_read <= sizeof (page->buffer));

        if (page->pixbuf_still_loading)
        {
            if (!gdk_pixbuf_loader_write (page->loader,
                                          (const guchar *) page->buffer,
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
                                       0,
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
        char *uri = g_file_get_uri (G_FILE (object));
        g_warning ("Error reading %s: %s", uri, error->message);
        g_free (uri);
        g_clear_error (&error);
    }

    if (done_reading)
    {
        load_finished (page);
        g_input_stream_close_async (stream,
                                    0,
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
    NautilusImagePropertiesPage *page;

    page = NAUTILUS_IMAGE_PROPERTIES_PAGE (callback_data);

    page->height = height;
    page->width = width;
    page->got_size = TRUE;
    page->pixbuf_still_loading = FALSE;
}

typedef struct
{
    NautilusImagePropertiesPage *page;
    NautilusFileInfo *info;
} FileOpenData;

static void
file_open_callback (GObject      *object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    FileOpenData *data = user_data;
    NautilusImagePropertiesPage *page = data->page;
    GFile *file;
    GFileInputStream *stream;
    GError *error;
    char *uri;

    file = G_FILE (object);
    uri = g_file_get_uri (file);

    error = NULL;
    stream = g_file_read_finish (file, res, &error);
    if (stream)
    {
        char *mime_type;

        mime_type = nautilus_file_info_get_mime_type (data->info);
        page->loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, &error);
        if (error != NULL)
        {
            g_warning ("Error creating loader for %s: %s", uri, error->message);
            g_clear_error (&error);
        }
        page->pixbuf_still_loading = TRUE;
        page->width = 0;
        page->height = 0;
        g_free (mime_type);

        g_signal_connect (page->loader,
                          "size-prepared",
                          G_CALLBACK (size_prepared_callback),
                          page);

        g_input_stream_read_async (G_INPUT_STREAM (stream),
                                   page->buffer,
                                   sizeof (page->buffer),
                                   0,
                                   page->cancellable,
                                   file_read_callback,
                                   page);

        g_object_unref (stream);
    }
    else
    {
        g_warning ("Error reading %s: %s", uri, error->message);
        g_clear_error (&error);
        load_finished (page);
    }

    g_free (uri);
    g_free (data);
}

static void
load_location (NautilusImagePropertiesPage *page,
               NautilusFileInfo            *info)
{
    GFile *file;
    char *uri;
    gchar *file_path;
    FileOpenData *data;
    GError *err;

    g_assert (NAUTILUS_IS_IMAGE_PROPERTIES_PAGE (page));
    g_assert (info != NULL);

    err = NULL;
    page->cancellable = g_cancellable_new ();

    uri = nautilus_file_info_get_uri (info);
    file = g_file_new_for_uri (uri);
    file_path = g_file_get_path (file);

    /* gexiv2 metadata init */
    page->md_ready = gexiv2_initialize ();
    if (!page->md_ready)
      {
        g_warning ("Unable to initialize gexiv2");
      }
    else
      {
        page->md = gexiv2_metadata_new ();
        if (file_path)
          {
            if (!gexiv2_metadata_open_path (page->md, file_path, &err))
              {
                g_warning ("gexiv2 metadata not supported for '%s': %s", file_path, err->message);
                g_clear_error (&err);
                page->md_ready = FALSE;
              }
          }
        else
          {
            page->md_ready = FALSE;
          }
      }

    data = g_new0 (FileOpenData, 1);
    data->page = page;
    data->info = info;

    g_file_read_async (file,
                       0,
                       page->cancellable,
                       file_open_callback,
                       data);

    g_object_unref (file);
    g_free (uri);
    g_free (file_path);
}

static void
nautilus_image_properties_page_class_init (NautilusImagePropertiesPageClass *class)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (class);

    object_class->finalize = nautilus_image_properties_page_finalize;
}

static void
nautilus_image_properties_page_init (NautilusImagePropertiesPage *page)
{
    GtkWidget *sw;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);
    gtk_box_set_homogeneous (GTK_BOX (page), FALSE);
    gtk_box_set_spacing (GTK_BOX (page), 0);
    gtk_container_set_border_width (GTK_CONTAINER (page), 0);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (sw), 0);
    gtk_widget_set_vexpand (GTK_WIDGET (sw), TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (page), sw, FALSE, TRUE, 2);

    page->grid = gtk_grid_new ();
    gtk_container_set_border_width (GTK_CONTAINER (page->grid), 6);
    gtk_orientable_set_orientation (GTK_ORIENTABLE (page->grid), GTK_ORIENTATION_VERTICAL);
    gtk_grid_set_row_spacing (GTK_GRID (page->grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (page->grid), 20);
    append_item (page, _("Loading…"), NULL);
    gtk_container_add (GTK_CONTAINER (sw), page->grid);

    gtk_widget_show_all (GTK_WIDGET (page));
}

static gboolean
is_mime_type_supported (const char *mime_type)
{
    gboolean supported;
    GSList *formats;
    GSList *l;

    supported = FALSE;
    formats = gdk_pixbuf_get_formats ();

    for (l = formats; supported == FALSE && l != NULL; l = l->next)
    {
        GdkPixbufFormat *format = l->data;
        char **mime_types = gdk_pixbuf_format_get_mime_types (format);
        int i;

        for (i = 0; mime_types[i] != NULL; i++)
        {
            if (strcmp (mime_types[i], mime_type) == 0)
            {
                supported = TRUE;
                break;
            }
        }
        g_strfreev (mime_types);
    }
    g_slist_free (formats);

    return supported;
}

static GList *
get_property_pages (NautilusPropertyPageProvider *provider,
                    GList                        *files)
{
    GList *pages;
    NautilusFileInfo *file;
    char *mime_type;

    /* Only show the property page if 1 file is selected */
    if (!files || files->next != NULL)
    {
        return NULL;
    }

    pages = NULL;
    file = NAUTILUS_FILE_INFO (files->data);

    mime_type = nautilus_file_info_get_mime_type (file);
    if (mime_type != NULL
        && is_mime_type_supported (mime_type))
    {
        NautilusImagePropertiesPage *page;
        NautilusPropertyPage *real_page;

        page = g_object_new (nautilus_image_properties_page_get_type (), NULL);
        load_location (page, file);

        real_page = nautilus_property_page_new
                        ("NautilusImagePropertiesPage::property_page",
                        gtk_label_new (_("Image")),
                        GTK_WIDGET (page));
        pages = g_list_append (pages, real_page);
    }

    g_free (mime_type);

    return pages;
}

static void
property_page_provider_iface_init (NautilusPropertyPageProviderIface *iface)
{
    iface->get_pages = get_property_pages;
}


static void
nautilus_image_properties_page_provider_init (NautilusImagePropertiesPageProvider *sidebar)
{
}

static void
nautilus_image_properties_page_provider_class_init (NautilusImagePropertiesPageProviderClass *class)
{
}

void
nautilus_image_properties_page_register (void)
{
    nautilus_module_add_type (nautilus_image_properties_page_provider_get_type ());
}
