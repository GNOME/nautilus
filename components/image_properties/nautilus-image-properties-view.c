/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2002 James Willcox
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
 * Author: James Willcox <jwillcox@gnome.org>
 */

#include <config.h>
#include "nautilus-image-properties-view.h"

#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <eel/eel-gnome-extensions.h>
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

struct NautilusImagePropertiesViewDetails {
	char *location;
	GtkWidget *vbox;
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
	PROP_URI,
};

static GObjectClass *parent_class = NULL;

static void
nautilus_image_properties_view_finalize (GObject *object)
{
	NautilusImagePropertiesView *view;

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (object);

	if (view->details->vfs_handle != NULL) {
		gnome_vfs_async_cancel (view->details->vfs_handle);
	}
	
	view->details->vfs_handle = NULL;
	g_free (view->details->location);

	g_free (view->details);

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
	gchar *camera_make, *camera_model;
	
	if (exifdata->ifd[0] && exifdata->ifd[0]->count) {
		camera_make = exifdata_get_tag_value_utf8 (exifdata, EXIF_TAG_MAKE);
		camera_model = exifdata_get_tag_value_utf8 (exifdata, EXIF_TAG_MODEL);
		if (camera_make != NULL) {
			g_string_append_printf (string, "<b>%s:</b> %s %s\n", 
						_("Camera"), 
						camera_make, 
						camera_model);
		}
                append_tag_value_pair (string, exifdata, EXIF_TAG_DATE_TIME, _("Date Taken"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_EXPOSURE_TIME, _("Exposure Time"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_EXPOSURE_PROGRAM, _("Exposure Program"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_APERTURE_VALUE, _("Aperture Value"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_METERING_MODE, _("Metering Mode"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_FLASH,_("Flash Fired"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_FOCAL_LENGTH,_("Focal Lenght"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_SHUTTER_SPEED_VALUE, _("Shutter Speed"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_ISO_SPEED_RATINGS, _("ISO Speed Rating"));
                append_tag_value_pair (string, exifdata, EXIF_TAG_SOFTWARE, _("Software"));

	}
}
#endif /*HAVE_EXIF*/

static void
load_finished (NautilusImagePropertiesView *view)
{
	GdkPixbufFormat *format;
	char *name, *desc;
	GString *str;

	if (view->details->got_size) {
		str = g_string_new (NULL);
		format = gdk_pixbuf_loader_get_format (view->details->loader);
	
		name = gdk_pixbuf_format_get_name (format);
		desc = gdk_pixbuf_format_get_description (format);
		g_string_append_printf (str, _("<b>Image Type:</b> %s (%s)\n<b>Resolution:</b> %dx%d pixels\n"),
					name, desc, view->details->width, view->details->height);
		g_free (name);
		g_free (desc);
		
#ifdef HAVE_EXIF
		append_exifdata_string (exif_loader_get_data (view->details->exifldr), str);
#endif /*HAVE_EXIF*/
		
		gtk_label_set_markup (GTK_LABEL (view->details->resolution), str->str);
		g_string_free (str, TRUE);
	} else {
		gtk_label_set_text (GTK_LABEL (view->details->resolution), _("Failed to load image information"));
	}

	if (view->details->loader != NULL) {
		gdk_pixbuf_loader_close (view->details->loader, NULL);
		g_object_unref (view->details->loader);
		view->details->loader = NULL;
	}
#ifdef HAVE_EXIF
	if (view->details->exifldr != NULL) {
		exif_loader_unref (view->details->exifldr);
		view->details->exifldr = NULL;
	}
#endif /*HAVE_EXIF*/
	
	if (view->details->vfs_handle != NULL) {
		gnome_vfs_async_close (view->details->vfs_handle, file_closed_callback, NULL);
		view->details->vfs_handle = NULL;
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
	NautilusImagePropertiesView *view;
#ifdef HAVE_EXIF
	int exif_still_loading;
#endif

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (callback_data);

	if (result == GNOME_VFS_OK && bytes_read != 0) {
#ifdef HAVE_EXIF
		exif_still_loading = exif_loader_write (view->details->exifldr,
				  		        buffer,
				  			bytes_read);
#endif
		if (view->details->pixbuf_still_loading) {
			if (!gdk_pixbuf_loader_write (view->details->loader,
					      	      buffer,
					      	      bytes_read,
					      	      NULL)) {
				view->details->pixbuf_still_loading = FALSE;
			}
		}
		if (view->details->pixbuf_still_loading
#ifdef HAVE_EXIF
		     && (exif_still_loading == 1)   
#endif
		   ) {
			gnome_vfs_async_read (view->details->vfs_handle,
					      view->details->buffer,
					      sizeof (view->details->buffer),
					      file_read_callback,
					      view);
			return;
		}
	}
	load_finished (view);
}

static void
size_prepared_callback (GdkPixbufLoader *loader, 
			int              width,
			int              height,
			gpointer         callback_data)
{
	NautilusImagePropertiesView *view;

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (callback_data);
	
	view->details->height = height;
	view->details->width = width;
	view->details->got_size = TRUE;
	view->details->pixbuf_still_loading = FALSE;
}

static void
file_opened_callback (GnomeVFSAsyncHandle *vfs_handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
	NautilusImagePropertiesView *view;

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (callback_data);
	
	if (result != GNOME_VFS_OK) {
		view->details->vfs_handle = NULL;
		return;
	}

	view->details->loader = gdk_pixbuf_loader_new ();
	view->details->pixbuf_still_loading = TRUE;
	view->details->width = 0;
	view->details->height = 0;
#ifdef HAVE_EXIF
	view->details->exifldr = exif_loader_new ();
#endif /*HAVE_EXIF*/

	g_signal_connect (view->details->loader, "size_prepared",
			  G_CALLBACK (size_prepared_callback), view);
	
	gnome_vfs_async_read (vfs_handle,
			      view->details->buffer,
			      sizeof (view->details->buffer),
			      file_read_callback,
			      view);
}


static void
load_location (NautilusImagePropertiesView *view,
	       const char *location)
{
	g_assert (NAUTILUS_IS_IMAGE_PROPERTIES_VIEW (view));
	g_assert (location != NULL);

	if (view->details->vfs_handle != NULL)
		gnome_vfs_async_cancel (view->details->vfs_handle);

	gnome_vfs_async_open (&view->details->vfs_handle,
			      location,
			      GNOME_VFS_OPEN_READ,
			      -2,
			      file_opened_callback,
			      view);
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg         *arg,
	      guint              arg_id,
	      CORBA_Environment *ev,
	      gpointer           user_data)
{
	NautilusImagePropertiesView *view = user_data;

	if (arg_id == PROP_URI) {
		CORBA_sequence_CORBA_string *uris;

		uris = CORBA_sequence_CORBA_string__alloc ();
		uris->_maximum = uris->_length = 1;
		uris->_buffer = CORBA_sequence_CORBA_string_allocbuf (uris->_length);
		uris->_buffer[0] = CORBA_string_dup (view->details->location);
		arg->_type = TC_CORBA_sequence_CORBA_string;
		arg->_value = uris;
	}
}

static void
set_property (BonoboPropertyBag *bag,
	      const BonoboArg   *arg,
	      guint              arg_id,
	      CORBA_Environment *ev,
	      gpointer           user_data)
{
	NautilusImagePropertiesView *view = user_data;

	if (arg_id == PROP_URI) {
		CORBA_sequence_CORBA_string *uris;
                
                uris = arg->_value;
		view->details->location = g_strdup (uris->_buffer[0]);
		load_location (view, view->details->location);
	}
}

static void
nautilus_image_properties_view_class_init (NautilusImagePropertiesViewClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	
	G_OBJECT_CLASS (class)->finalize = nautilus_image_properties_view_finalize;
}

static void
nautilus_image_properties_view_init (NautilusImagePropertiesView *view)
{
	BonoboPropertyBag *pb;

	view->details = g_new0 (NautilusImagePropertiesViewDetails, 1);

	view->details->vbox = gtk_vbox_new (FALSE, 2);
	view->details->resolution = gtk_label_new (_("loading..."));

	gtk_box_pack_start (GTK_BOX (view->details->vbox),
			    view->details->resolution,
			    FALSE, TRUE, 2);

	gtk_widget_show_all (view->details->vbox);
	
	bonobo_control_construct (BONOBO_CONTROL (view), view->details->vbox);

	pb = bonobo_property_bag_new (get_property, set_property,
				      view);
	bonobo_property_bag_add (pb, "uris", 0, TC_CORBA_sequence_CORBA_string,
				 NULL, _("URI currently displayed"), 0);
	bonobo_control_set_properties (BONOBO_CONTROL (view),
				       BONOBO_OBJREF (pb), NULL);
	bonobo_object_release_unref (BONOBO_OBJREF (pb), NULL);
}

BONOBO_TYPE_FUNC (NautilusImagePropertiesView, BONOBO_TYPE_CONTROL, nautilus_image_properties_view);
