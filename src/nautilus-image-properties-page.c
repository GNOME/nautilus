/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2004 Red Hat, Inc
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include "nautilus-image-properties-page.h"

#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <eel/eel-gnome-extensions.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>
#include <libnautilus-private/nautilus-module.h>
#include <string.h>

#ifdef HAVE_EXIF
  #include <libexif/exif-data.h>
  #include <libexif/exif-ifd.h>
  #include <libexif/exif-loader.h>
  #include <gtk/gtkliststore.h>
  #include <gtk/gtktreestore.h>
  #include <gtk/gtktreeview.h>
  #include <gtk/gtkscrolledwindow.h>
  #include <gtk/gtkcellrenderertext.h>
  #include <eel/eel-vfs-extensions.h>
#endif

#define LOAD_BUFFER_SIZE 8192

struct NautilusImagePropertiesPageDetails {
	char *location;
	GtkWidget *resolution;
	GnomeVFSAsyncHandle *vfs_handle;
	GdkPixbufLoader *loader;
	gboolean got_size;
	gboolean pixbuf_still_loading;
	char buffer[LOAD_BUFFER_SIZE];
	int width;
	int height;
#ifdef HAVE_EXIF
	ExifLoader *exifldr;
#endif /*HAVE_EXIF*/
};

#ifdef HAVE_EXIF
struct ExifAttribute {
	ExifTag tag;
	char *value;
	gboolean found;
};
#endif /*HAVE_EXIF*/

enum {
	PROP_URI
};

typedef struct {
        GObject parent;
} NautilusImagePropertiesPageProvider;

typedef struct {
        GObjectClass parent;
} NautilusImagePropertiesPageProviderClass;


static GObjectClass *parent_class = NULL;

static GType nautilus_image_properties_page_provider_get_type (void);
static void  property_page_provider_iface_init                (NautilusPropertyPageProviderIface *iface);


G_DEFINE_TYPE (NautilusImagePropertiesPage, nautilus_image_properties_page, GTK_TYPE_VBOX);

G_DEFINE_TYPE_WITH_CODE (NautilusImagePropertiesPageProvider, nautilus_image_properties_page_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER,
						property_page_provider_iface_init));

static void
nautilus_image_properties_page_finalize (GObject *object)
{
	NautilusImagePropertiesPage *page;

	page = NAUTILUS_IMAGE_PROPERTIES_PAGE (object);

	if (page->details->vfs_handle != NULL) {
		gnome_vfs_async_cancel (page->details->vfs_handle);
	}
	
	page->details->vfs_handle = NULL;
	g_free (page->details->location);

	g_free (page->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
file_closed_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
}

#ifdef HAVE_EXIF
static char *
exif_string_to_utf8 (const char *exif_str)
{
	char *utf8_str;
	
	if (g_utf8_validate (exif_str, -1, NULL)) {
		return g_strdup (exif_str);
	}
	
	utf8_str = g_locale_to_utf8 (exif_str, -1, NULL, NULL, NULL);
	if (utf8_str != NULL) {
		return utf8_str;
	}
	
	return eel_make_valid_utf8 (exif_str);
}

static void
exif_content_callback (ExifContent *content, gpointer data)
{
	struct ExifAttribute *attribute;

	attribute = (struct ExifAttribute *)data;
	if (attribute->found) {
		return;
	}

        attribute->value = g_strdup (exif_content_get_value (content, attribute->tag));
	if (attribute->value != NULL) {
		attribute->found = TRUE;
	}
}

static char *
exifdata_get_tag_name_utf8 (ExifTag tag) 
{
	return exif_string_to_utf8 (exif_tag_get_name (tag));
}

static char *
exifdata_get_tag_value_utf8 (ExifData *data, ExifTag tag) 
{
	struct ExifAttribute attribute;
	char *utf8_value;

	attribute.tag = tag;
	attribute.value = NULL;
	attribute.found = FALSE;
	
	exif_data_foreach_content (data, exif_content_callback, &attribute);

	if (attribute.found) {
		utf8_value = exif_string_to_utf8 (attribute.value);
		g_free (attribute.value);
	} else {
		utf8_value = NULL;
	}

	return utf8_value;
}

static void
append_tag_value_pair (GString *string, ExifData *data, ExifTag tag, gchar *description) 
{
        char *utf_attribute;
        char *utf_value;
 
	utf_attribute = exifdata_get_tag_name_utf8 (tag);
	utf_value = exifdata_get_tag_value_utf8 (data, tag);

	if ((utf_attribute == NULL) || (utf_value == NULL)) {
		g_free (utf_attribute);
		g_free (utf_value);
   		return;
	}

	g_string_append_printf (string, "<b>%s:</b> %s\n", (description != NULL) ? description : utf_attribute, utf_value);

        g_free (utf_attribute);
        g_free (utf_value);
}

static void
append_exifdata_string (ExifData *exifdata, GString *string)
{
	if (exifdata->ifd[0] && exifdata->ifd[0]->count) {
                append_tag_value_pair (string, exifdata, EXIF_TAG_MAKE, _("Camera Brand"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_MODEL, _("Camera Model"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_DATE_TIME, _("Date Taken"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_EXPOSURE_TIME, _("Exposure Time"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_EXPOSURE_PROGRAM, _("Exposure Program"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_APERTURE_VALUE, _("Aperture Value"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_METERING_MODE, _("Metering Mode"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_FLASH,_("Flash Fired"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_FOCAL_LENGTH,_("Focal Length"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_SHUTTER_SPEED_VALUE, _("Shutter Speed"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_ISO_SPEED_RATINGS, _("ISO Speed Rating"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_SOFTWARE, _("Software"));

	}
}
#endif /*HAVE_EXIF*/

static void
load_finished (NautilusImagePropertiesPage *page)
{
	GdkPixbufFormat *format;
	char *name, *desc;
	GString *str;

	if (page->details->got_size) {
		str = g_string_new (NULL);
		format = gdk_pixbuf_loader_get_format (page->details->loader);
	
		name = gdk_pixbuf_format_get_name (format);
		desc = gdk_pixbuf_format_get_description (format);
		g_string_append_printf (str, ngettext ("<b>Image Type:</b> %s (%s)\n<b>Resolution:</b> %dx%d pixels\n",
						       "<b>Image Type:</b> %s (%s)\n<b>Resolution:</b> %dx%d pixels\n",
						       page->details->height),
					name, desc, page->details->width, page->details->height);
		g_free (name);
		g_free (desc);
		
#ifdef HAVE_EXIF
		append_exifdata_string (exif_loader_get_data (page->details->exifldr), str);
#endif /*HAVE_EXIF*/
		
		gtk_label_set_markup (GTK_LABEL (page->details->resolution), str->str);
		gtk_label_set_selectable (GTK_LABEL (page->details->resolution), TRUE);
		g_string_free (str, TRUE);
	} else {
		gtk_label_set_text (GTK_LABEL (page->details->resolution), _("Failed to load image information"));
	}

	if (page->details->loader != NULL) {
		gdk_pixbuf_loader_close (page->details->loader, NULL);
		g_object_unref (page->details->loader);
		page->details->loader = NULL;
	}
#ifdef HAVE_EXIF
	if (page->details->exifldr != NULL) {
		exif_loader_unref (page->details->exifldr);
		page->details->exifldr = NULL;
	}
#endif /*HAVE_EXIF*/
	
	if (page->details->vfs_handle != NULL) {
		gnome_vfs_async_close (page->details->vfs_handle, file_closed_callback, NULL);
		page->details->vfs_handle = NULL;
	}
}

static void
file_read_callback (GnomeVFSAsyncHandle *vfs_handle,
		    GnomeVFSResult result,
		    gpointer buffer,
		    GnomeVFSFileSize bytes_requested,
		    GnomeVFSFileSize bytes_read,
		    gpointer callback_data)
{
	NautilusImagePropertiesPage *page;
#ifdef HAVE_EXIF
	int exif_still_loading;
#endif

	page = NAUTILUS_IMAGE_PROPERTIES_PAGE (callback_data);

	if (result == GNOME_VFS_OK && bytes_read != 0) {
#ifdef HAVE_EXIF
		exif_still_loading = exif_loader_write (page->details->exifldr,
				  		        buffer,
				  			bytes_read);
#endif
		if (page->details->pixbuf_still_loading) {
			if (!gdk_pixbuf_loader_write (page->details->loader,
					      	      buffer,
					      	      bytes_read,
					      	      NULL)) {
				page->details->pixbuf_still_loading = FALSE;
			}
		}
		if (page->details->pixbuf_still_loading
#ifdef HAVE_EXIF
		     && (exif_still_loading == 1)   
#endif
		   ) {
			gnome_vfs_async_read (page->details->vfs_handle,
					      page->details->buffer,
					      sizeof (page->details->buffer),
					      file_read_callback,
					      page);
			return;
		}
	}
	load_finished (page);
}

static void
size_prepared_callback (GdkPixbufLoader *loader, 
			int              width,
			int              height,
			gpointer         callback_data)
{
	NautilusImagePropertiesPage *page;

	page = NAUTILUS_IMAGE_PROPERTIES_PAGE (callback_data);
	
	page->details->height = height;
	page->details->width = width;
	page->details->got_size = TRUE;
	page->details->pixbuf_still_loading = FALSE;
}

static void
file_opened_callback (GnomeVFSAsyncHandle *vfs_handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	NautilusImagePropertiesPage *page;

	page = NAUTILUS_IMAGE_PROPERTIES_PAGE (callback_data);
	
	if (result != GNOME_VFS_OK) {
		page->details->vfs_handle = NULL;
		return;
	}

	page->details->loader = gdk_pixbuf_loader_new ();
	page->details->pixbuf_still_loading = TRUE;
	page->details->width = 0;
	page->details->height = 0;
#ifdef HAVE_EXIF
	page->details->exifldr = exif_loader_new ();
#endif /*HAVE_EXIF*/

	g_signal_connect (page->details->loader, "size_prepared",
			  G_CALLBACK (size_prepared_callback), page);
	
	gnome_vfs_async_read (vfs_handle,
			      page->details->buffer,
			      sizeof (page->details->buffer),
			      file_read_callback,
			      page);
}


static void
load_location (NautilusImagePropertiesPage *page,
	       const char *location)
{
	g_assert (NAUTILUS_IS_IMAGE_PROPERTIES_PAGE (page));
	g_assert (location != NULL);

	if (page->details->vfs_handle != NULL)
		gnome_vfs_async_cancel (page->details->vfs_handle);

	gnome_vfs_async_open (&page->details->vfs_handle,
			      location,
			      GNOME_VFS_OPEN_READ,
			      -2,
			      file_opened_callback,
			      page);
}

static void
nautilus_image_properties_page_class_init (NautilusImagePropertiesPageClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	
	G_OBJECT_CLASS (class)->finalize = nautilus_image_properties_page_finalize;
}

static void
nautilus_image_properties_page_init (NautilusImagePropertiesPage *page)
{
	page->details = g_new0 (NautilusImagePropertiesPageDetails, 1);

	gtk_box_set_homogeneous (GTK_BOX (page), FALSE);
	gtk_box_set_spacing (GTK_BOX (page), 2);
	gtk_container_set_border_width (GTK_CONTAINER (page), 6);
	
	page->details->resolution = gtk_label_new (_("loading..."));
	gtk_misc_set_alignment (GTK_MISC (page->details->resolution),
				0,
				0);

	gtk_box_pack_start (GTK_BOX (page),
			    page->details->resolution,
			    FALSE, TRUE, 2);

	gtk_widget_show_all (GTK_WIDGET (page));
}

/* nautilus_property_page_provider_get_pages
 *  
 * This function is called by Nautilus when it wants property page
 * items from the extension.
 *
 * This function is called in the main thread before a property page
 * is shown, so it should return quickly.
 * 
 * The function should return a GList of allocated NautilusPropertyPage
 * items.
 */
static GList *
get_property_pages (NautilusPropertyPageProvider *provider,
                    GList *files)
{
	GList *pages;
	NautilusPropertyPage *real_page;
	NautilusFileInfo *file;
        char *uri;
	NautilusImagePropertiesPage *page;
	
	/* Only show the property page if 1 file is selected */
	if (!files || files->next != NULL) {
		return NULL;
	}

	file = NAUTILUS_FILE_INFO (files->data);
	
	if (!
	    (nautilus_file_info_is_mime_type (file, "image/x-bmp") ||
	     nautilus_file_info_is_mime_type (file, "image/x-ico") ||
	     nautilus_file_info_is_mime_type (file, "image/jpeg") ||
	     nautilus_file_info_is_mime_type (file, "image/gif") ||
	     nautilus_file_info_is_mime_type (file, "image/png") ||
	     nautilus_file_info_is_mime_type (file, "image/pnm") ||
	     nautilus_file_info_is_mime_type (file, "image/ras") ||
	     nautilus_file_info_is_mime_type (file, "image/tga") ||
	     nautilus_file_info_is_mime_type (file, "image/tiff") ||
	     nautilus_file_info_is_mime_type (file, "image/wbmp") ||
	     nautilus_file_info_is_mime_type (file, "image/x-xbitmap") ||
	     nautilus_file_info_is_mime_type (file, "image/x-xpixmap"))) {
		return NULL;
	}
	
	pages = NULL;
	
        uri = nautilus_file_info_get_uri (file);
	
	page = g_object_new (nautilus_image_properties_page_get_type (), NULL);
        page->details->location = uri;
	load_location (page, page->details->location);

        real_page = nautilus_property_page_new
                ("NautilusImagePropertiesPage::property_page", 
                 gtk_label_new (_("Image")),
                 GTK_WIDGET (page));
        pages = g_list_append (pages, real_page);

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

