/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 */

/* this is the implementation of the vcard component, which display a vcard graphically
 */
 
#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>

#include <bonobo.h>

#include "vcard.h"
#include "nautilus-vcard.h"

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>

#include <libnautilus/nautilus-view.h>

#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <libnautilus-extensions/nautilus-font-factory.h>

/* private instance variables */
struct _NautilusVCardDetails {
	char *vcard_uri;
	char *vcard_data;

	EelReadFileHandle *load_file_handle;
	EelPixbufLoadHandle *load_image_handle;
	
	BonoboObject *control;
	EelScalableFont *font;	

	GdkPixbuf *logo;
};

static void nautilus_vcard_initialize_class (NautilusVCardClass *klass);
static void nautilus_vcard_initialize (NautilusVCard *view);
static void nautilus_vcard_destroy (GtkObject *object);

static void nautilus_vcard_draw (GtkWidget *widget, GdkRectangle *box);
static int  nautilus_vcard_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean nautilus_vcard_button_press_event (GtkWidget *widget, GdkEventButton *event);
static gboolean nautilus_vcard_motion_event (GtkWidget *widget, GdkEventMotion *event);
static gboolean nautilus_vcard_leave_event (GtkWidget *widget, GdkEventCrossing *event);
static void nautilus_vcard_size_request (GtkWidget *widget, GtkRequisition *request);

static void nautilus_vcard_set_uri (NautilusVCard *vcard, const char *uri);


EEL_DEFINE_CLASS_BOILERPLATE (NautilusVCard,
                                   nautilus_vcard,
                                   GTK_TYPE_EVENT_BOX)

#define MINIMUM_DRAW_SIZE 4

static void
nautilus_vcard_initialize_class (NautilusVCardClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	
	object_class->destroy = nautilus_vcard_destroy;
	
	widget_class->draw = nautilus_vcard_draw;
	widget_class->expose_event = nautilus_vcard_expose;
	widget_class->button_press_event = nautilus_vcard_button_press_event;
	widget_class->motion_notify_event = nautilus_vcard_motion_event;
	widget_class->leave_notify_event = nautilus_vcard_leave_event;
	widget_class->size_request = nautilus_vcard_size_request;
}

/* routines to handle setting and getting the configuration properties of the Bonobo control */

enum {
	CONFIGURATION
} MyArgs;


static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer user_data)
{
	NautilusVCard *vcard = NAUTILUS_VCARD (user_data);

	switch (arg_id) {

		case CONFIGURATION:
		{
			BONOBO_ARG_SET_STRING (arg, vcard->details->vcard_uri);
			break;
		}

		default:
			g_warning ("Unhandled arg %d", arg_id);
			break;
	}
}

static void
set_bonobo_properties (BonoboPropertyBag *bag,
			const BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer user_data)
{
	NautilusVCard *vcard = NAUTILUS_VCARD (user_data);

	switch (arg_id) {

		case CONFIGURATION:
		{
			char *uri;

			uri = BONOBO_ARG_GET_STRING (arg);
			nautilus_vcard_set_uri (vcard, uri);
			
			break;
		}

		default:
			g_warning ("Unhandled arg %d", arg_id);
			break;
	}
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */
static void
nautilus_vcard_initialize (NautilusVCard *vcard)
{
	GtkWidget *frame;
	BonoboPropertyBag *property_bag;
		
	vcard->details = g_new0 (NautilusVCardDetails, 1);

	/* set up the font */
	vcard->details->font = eel_scalable_font_get_default_font ();
	
	/* receive mouse motion events */
	gtk_widget_add_events (GTK_WIDGET (vcard), GDK_POINTER_MOTION_MASK);

	/* embed it into a frame */		
	frame = gtk_frame_new (NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_OUT);
  	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vcard));
	
	/* make the bonobo control */
	vcard->details->control = (BonoboObject*) bonobo_control_new (GTK_WIDGET (frame));
	
	/* attach a property bag with the configure property */
	property_bag = bonobo_property_bag_new (get_bonobo_properties, set_bonobo_properties, vcard);
	bonobo_control_set_properties (BONOBO_CONTROL(vcard->details->control),property_bag);
	bonobo_object_unref (BONOBO_OBJECT (property_bag));

	bonobo_property_bag_add (property_bag, "configuration", CONFIGURATION, BONOBO_ARG_STRING, NULL,
				 "VCard Configuration", BONOBO_PROPERTY_WRITEABLE);
			 	
	/* show the view itself */	
	gtk_widget_show (GTK_WIDGET (vcard));
}


static void
nautilus_vcard_destroy (GtkObject *object)
{
	NautilusVCard *vcard;
	
	vcard = NAUTILUS_VCARD (object);
	g_free (vcard->details->vcard_uri);
	g_free (vcard->details->vcard_data);
	
	if (vcard->details->load_file_handle != NULL) {
		eel_read_file_cancel (vcard->details->load_file_handle);
	}
	
	if (vcard->details->load_image_handle != NULL) {
		eel_cancel_gdk_pixbuf_load (vcard->details->load_image_handle);
	}
			

	if (vcard->details->logo != NULL) {
		gdk_pixbuf_unref (vcard->details->logo);
	}	
	
	if (vcard->details->font) {
		gtk_object_unref (GTK_OBJECT (vcard->details->font));
	}
	
	g_free (vcard->details);

	/* Chain destroy */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* get associated Bonobo control */
BonoboObject *
nautilus_vcard_get_control (NautilusVCard *vcard)
{
	return vcard->details->control;
}

static void
vcard_logo_callback (GnomeVFSResult  error, GdkPixbuf *pixbuf, gpointer callback_data)
{
	NautilusVCard *vcard;
	GdkPixbuf *scaled_pixbuf;
	
	vcard = NAUTILUS_VCARD (callback_data);
	vcard->details->load_image_handle = NULL;
	
	if (vcard->details->logo) {
		gdk_pixbuf_unref (vcard->details->logo);
	}
	
	if (pixbuf != NULL) {
		gdk_pixbuf_ref (pixbuf);
		
		scaled_pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, 128, 80);
		gdk_pixbuf_unref (pixbuf);
		vcard->details->logo = scaled_pixbuf;
		gtk_widget_queue_draw (GTK_WIDGET (vcard));
	}
}

/* completion routine invoked when we've loaded the vcard file uri.  */

static void
vcard_read_done_callback (GnomeVFSResult result,
			 GnomeVFSFileSize file_size,
			 char *file_contents,
			 gpointer callback_data)
{
	char *logo_uri;
	NautilusVCard *vcard;
	
	vcard = NAUTILUS_VCARD (callback_data);
	vcard->details->load_file_handle = NULL;

	/* make sure the read was successful */
	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		return;
	}

	/* free old data if any */
	if (vcard->details->vcard_data) {
		g_free (vcard->details->vcard_data);
	}
	
	/* set up the vcard data */
	vcard->details->vcard_data = g_realloc (file_contents, file_size + 1);
	vcard->details->vcard_data[file_size] = '\0';
			
	/* extract the image uri and, if found, load it asynchronously */
	logo_uri = vcard_logo (vcard->details->vcard_data);
	g_message ("logo uri is %s", logo_uri);
	if (logo_uri != NULL) {
		vcard->details->load_image_handle = eel_gdk_pixbuf_load_async (logo_uri, vcard_logo_callback, vcard);
		g_free (logo_uri);
	}
						
	/* schedule a redraw to reflect the new contents */
	gtk_widget_queue_draw (GTK_WIDGET (vcard));
}

/* load the vcard  asynchronously */
static void
load_vcard (NautilusVCard *vcard)
{
	/* load the uri asynchrounously, calling a completion routine when completed */
	vcard->details->load_file_handle = eel_read_entire_file_async (vcard->details->vcard_uri, vcard_read_done_callback, vcard);
}

/* set the uri and load it */
static void
nautilus_vcard_set_uri (NautilusVCard *vcard, const char *uri)
{

	if (eel_strcmp (vcard->details->vcard_uri, uri) == 0) {
		return;
	}
	
	if (vcard->details->vcard_uri != NULL) {
		g_free (vcard->details->vcard_uri);
		vcard->details->vcard_uri = NULL;
	}

	if (uri != NULL) {
		vcard->details->vcard_uri = g_strdup (uri);
		load_vcard (vcard);	
	}
}

/* convenience routine to composite an image with the proper clipping */
static void
vcard_pixbuf_composite (GdkPixbuf *source, GdkPixbuf *destination, int x_offset, int y_offset, int alpha)
{
	int source_width, source_height, dest_width, dest_height;
	double float_x_offset, float_y_offset;
	
	source_width  = gdk_pixbuf_get_width (source);
	source_height = gdk_pixbuf_get_height (source);
	dest_width  = gdk_pixbuf_get_width (destination);
	dest_height = gdk_pixbuf_get_height (destination);
	
	float_x_offset = x_offset;
	float_y_offset = y_offset;
	
	/* clip to the destination size */
	if ((x_offset + source_width) > dest_width) {
		source_width = dest_width - x_offset;
	}
	if ((y_offset + source_height) > dest_height) {
		source_height = dest_height - y_offset;
	}
	
	gdk_pixbuf_composite (source, destination, x_offset, y_offset, source_width, source_height,
					float_x_offset, float_y_offset, 1.0, 1.0, GDK_PIXBUF_ALPHA_BILEVEL, alpha);
}



/* draw the logo image */
static int
draw_vcard_logo_image (NautilusVCard *vcard, GdkPixbuf *pixbuf, int offset)
{
	GtkWidget *widget;
	int logo_width, logo_height;
	int v_offset;
	
	widget = GTK_WIDGET (vcard);
	v_offset = offset;
	
	if (vcard->details->logo != NULL) {
		logo_width = gdk_pixbuf_get_width (vcard->details->logo);
		logo_height = gdk_pixbuf_get_height (vcard->details->logo);

		vcard_pixbuf_composite (vcard->details->logo, pixbuf, 2, v_offset, 255);
		v_offset += logo_height + 2;
	}

	return v_offset;
}

/* draw the name and title */
static int
draw_vcard_name_and_title (NautilusVCard *vcard, GdkPixbuf *pixbuf, int v_offset)
{
	int name_len, title_len;
	char *name, *title;
	GtkWidget *widget;
        EelDimensions name_dimensions;
        EelDimensions title_dimensions;
	
	if (vcard->details->font == NULL) {
		return v_offset;
	}
	
	widget = GTK_WIDGET (vcard);

	/* extract the name and title */
	name = vcard_full_name (vcard->details->vcard_data);  
	title = vcard_title (vcard->details->vcard_data);  

	/* first, measure the name */
	if (name != NULL) {
		name_len = strlen (name);
		name_dimensions = eel_scalable_font_measure_text (vcard->details->font, 
                                                                  18,
                                                                  name, name_len);
	
		/* draw the name into the pixbuf using anti-aliased text */	
		eel_scalable_font_draw_text (vcard->details->font, pixbuf, 
					  4, v_offset,
					  NULL,
					  18,
					  name, name_len,
					  EEL_RGB_COLOR_BLACK,
					  EEL_OPACITY_FULLY_OPAQUE);
		v_offset += name_dimensions.height + 4;

		g_free (name);
		
		if (title != NULL) {
			title_len = strlen (title);
			title_dimensions = eel_scalable_font_measure_text (vcard->details->font, 
					     14,
					     title, title_len);
	
			/* draw the name into the pixbuf using anti-aliased text */	
			eel_scalable_font_draw_text (vcard->details->font, pixbuf, 
					  4, v_offset,
					  NULL,
					  14,
					  title, title_len,
					  EEL_RGB_COLOR_BLACK,
					  EEL_OPACITY_FULLY_OPAQUE);
			v_offset += title_dimensions.height + 4;
			
			g_free (title);
		}
	}
	return v_offset;
}

/* draw the addresses and phone numbers associated with the vcard */
static int
draw_vcard_addresses (NautilusVCard *vcard, GdkPixbuf *pixbuf, int v_offset)
{
	return v_offset;
}

/* handle drawing the control */
static void
nautilus_vcard_draw (GtkWidget *widget, GdkRectangle *box)
{
	NautilusVCard *control;
	GdkPixbuf *temp_pixbuf;
	int width, height, v_offset;

	/* allocate a pixbuf to draw into */
	width = widget->allocation.width;
	height = widget->allocation.height;
	
	
	temp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
		
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_VCARD (widget));

	control = NAUTILUS_VCARD (widget);

	/* draw the background */
	eel_gdk_pixbuf_fill_rectangle_with_color (temp_pixbuf, NULL, 0xFFEFEFEF);

	/* draw the logo, if any */
	v_offset = draw_vcard_logo_image (control, temp_pixbuf, 2);
	
	/* draw the name and title */
	v_offset = draw_vcard_name_and_title (control, temp_pixbuf, v_offset);
	
	v_offset += 6;
	/* draw the addresses */
	v_offset = draw_vcard_addresses (control, temp_pixbuf, v_offset);
	
	/* blit the resultingpixbuf to the drawable, then release it */
	gdk_pixbuf_render_to_drawable_alpha (temp_pixbuf,
					widget->window,
					0, 0,
					widget->allocation.x, widget->allocation.y,
					width, height,
					GDK_PIXBUF_ALPHA_BILEVEL, 128,
					GDK_RGB_DITHER_MAX,
					0, 0);

	gdk_pixbuf_unref (temp_pixbuf);
}

/* handle expose events */
static int
nautilus_vcard_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GdkRectangle box;
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_VCARD (widget), FALSE);

	box.x = 0; box.y = 0;
	box.width = widget->allocation.width;
	box.height = widget->allocation.height;

	nautilus_vcard_draw (widget, &box);
	return FALSE;
}


/* handle mouse motion events by maintaining the prelight state */
static gboolean
nautilus_vcard_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
	int x, y;
	int which_item, item_count;
	NautilusVCard *vcard;
	
	vcard = NAUTILUS_VCARD (widget);

	gtk_widget_get_pointer (widget, &x, &y);
	which_item = 0;
	item_count = 0;
	
	if (which_item < 0 || which_item >= item_count) {
		which_item = -1;
	}
	return TRUE;
}

/* handle size requests by requesting a fixed size */
static void
nautilus_vcard_size_request (GtkWidget *widget, GtkRequisition *request)
{
	request->width = 220;
	request->height = 140;
}

/* handle leave events by cancelling any prelighting */
static gboolean
nautilus_vcard_leave_event (GtkWidget *widget, GdkEventCrossing *event)
{
	NautilusVCard *vcard;
	
	vcard = NAUTILUS_VCARD (widget);
	return TRUE;
}

/* handle button press events */
static gboolean
nautilus_vcard_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	GList *selected_item;
	NautilusVCard *vcard;
	char *command;
	int result, which_item;

	vcard = NAUTILUS_VCARD (widget);
	if (event->y < widget->allocation.y) {
		command = g_strdup_printf ("nautilus %s", vcard->details->vcard_uri);
		result = system (command);
		g_free (command);
	} else {
		which_item = (event->y - widget->allocation.y ) / 16;
		if (which_item < 0) {
			selected_item = 0;
						
		}
	}
	
	return FALSE;
}

