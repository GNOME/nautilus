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

#define LOAD_BUFFER_SIZE 8192

struct NautilusImagePropertiesViewDetails {
	char *location;
	GtkWidget *vbox;
	GtkWidget *resolution;
	GnomeVFSAsyncHandle *vfs_handle;
	GdkPixbufLoader *loader;
	gboolean got_size;
	char buffer[LOAD_BUFFER_SIZE];
};

enum {
	PROP_URI,
};

static void
nautilus_image_properties_view_finalize (GObject *object)
{
	NautilusImagePropertiesView *view;
	
	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (object);

	if (view->details->vfs_handle != NULL)
		gnome_vfs_async_cancel (view->details->vfs_handle);
	
	view->details->vfs_handle = NULL;

	g_free (view->details);

	G_OBJECT_CLASS (object)->finalize (object);
}

static void
file_closed_callback (GnomeVFSAsyncHandle *handle,
		      GnomeVFSResult result,
		      gpointer callback_data)
{
}

static void
load_finished (NautilusImagePropertiesView *view)
{
	if (view->details->loader != NULL) {
		gdk_pixbuf_loader_close (view->details->loader, NULL);
		g_object_unref (view->details->loader);
		view->details->loader = NULL;
	}
	
	if (view->details->vfs_handle != NULL) {
		gnome_vfs_async_close (view->details->vfs_handle, file_closed_callback, NULL);
		view->details->vfs_handle = NULL;
	}

	if (!view->details->got_size) {
		gtk_label_set_text (GTK_LABEL (view->details->resolution), _("Failed to load image information"));
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

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (callback_data);

	if (result == GNOME_VFS_OK && bytes_read != 0) {
		if (!gdk_pixbuf_loader_write (view->details->loader,
					      buffer,
					      bytes_read,
					      NULL)) {
			result = GNOME_VFS_ERROR_WRONG_FORMAT;
		} else if (!view->details->got_size) {
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
	GdkPixbufFormat *format;
	char *str;
	char *name, *desc;

	view = NAUTILUS_IMAGE_PROPERTIES_VIEW (callback_data);

	format = gdk_pixbuf_loader_get_format (loader);
	
	name = gdk_pixbuf_format_get_name (format);
	desc = gdk_pixbuf_format_get_description (format);
	str = g_strdup_printf (_("<b>Image Type:</b> %s (%s)\n<b>Resolution:</b> %dx%d pixels\n"),
				 name, desc, width, height);
	gtk_label_set_markup (GTK_LABEL (view->details->resolution), str);
	g_free (str);
	g_free (name);
	g_free (desc);
	
	view->details->got_size = TRUE;
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
		BONOBO_ARG_SET_STRING (arg, view->details->location);
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
		load_location (view, BONOBO_ARG_GET_STRING (arg));
	}
}

static void
nautilus_image_properties_view_class_init (NautilusImagePropertiesViewClass *class)
{
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
	bonobo_property_bag_add (pb, "URI", 0, BONOBO_ARG_STRING,
				 NULL, _("URI currently displayed"), 0);
	bonobo_control_set_properties (BONOBO_CONTROL (view),
				       BONOBO_OBJREF (pb), NULL);
	bonobo_object_release_unref (BONOBO_OBJREF (pb), NULL);
}

BONOBO_TYPE_FUNC (NautilusImagePropertiesView, BONOBO_TYPE_CONTROL, nautilus_image_properties_view);
