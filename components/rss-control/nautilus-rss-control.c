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

/* this is the implementation of the rss control, which fetches an rss file through a uri, and
 * displays it in the widget.
 */
 
#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>

#include <bonobo.h>

#include "nautilus-rss-control.h"
#include <ghttp.h>

#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>

#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-xml-extensions.h>
#include <libnautilus-extensions/nautilus-font-factory.h>

struct _NautilusRSSControlDetails {
	char* rss_uri;
	BonoboObject *control;
	NautilusScalableFont *font;	

	char* title;
	GdkPixbuf *logo;
	GdkPixbuf *bullet;
	GList *items;
};


static void nautilus_rss_control_initialize_class (NautilusRSSControlClass *klass);
static void nautilus_rss_control_initialize (NautilusRSSControl *view);
static void nautilus_rss_control_destroy (GtkObject *object);

static void nautilus_rss_control_draw (GtkWidget *widget, GdkRectangle *box);
static int	nautilus_rss_control_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean nautilus_rss_control_button_press_event (GtkWidget *widget, GdkEventButton *event);
static void nautilus_rss_control_set_uri (NautilusRSSControl *rss_control, const char *uri);



NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusRSSControl,
                                   nautilus_rss_control,
                                   GTK_TYPE_EVENT_BOX)


static void
nautilus_rss_control_initialize_class (NautilusRSSControlClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	
	object_class->destroy = nautilus_rss_control_destroy;
	
	widget_class->draw = nautilus_rss_control_draw;
	widget_class->expose_event = nautilus_rss_control_expose;
	widget_class->button_press_event = nautilus_rss_control_button_press_event;
}

/* routines to handle setting and getting the configuration properties of the Bonobo control */

enum {
	PLACEHOLDER,
	CONFIGURATION
} MyArgs;


static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			gpointer user_data)
{
	NautilusRSSControl *rss_control = NAUTILUS_RSS_CONTROL (user_data);

	switch (arg_id) {

		case CONFIGURATION:
		{
			BONOBO_ARG_SET_STRING (arg, rss_control->details->rss_uri);
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
			gpointer user_data)
{
	NautilusRSSControl *rss_control = NAUTILUS_RSS_CONTROL (user_data);

	switch (arg_id) {

		case CONFIGURATION:
		{
			char *uri;

			uri = BONOBO_ARG_GET_STRING (arg);
			nautilus_rss_control_set_uri (rss_control, uri);
			
			break;
		}

		default:
			g_warning ("Unhandled arg %d", arg_id);
			break;
	}
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */
static void
nautilus_rss_control_initialize (NautilusRSSControl *rss_control)
{
	GtkWidget *frame;
	char *bullet_path;
	BonoboPropertyBag *property_bag;
		
	rss_control->details = g_new0 (NautilusRSSControlDetails, 1);

	/* set up the font */
	rss_control->details->font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("helvetica", "medium", NULL, NULL));

	/* load the bullet used to display the items */
	bullet_path = nautilus_pixmap_file ("bullet.png");
	rss_control->details->bullet = gdk_pixbuf_new_from_file (bullet_path);
	g_free (bullet_path);
		
	/* make the bonobo control */
	frame = gtk_frame_new (NULL);
  	gtk_frame_set_shadow_type(GTK_FRAME (frame), GTK_SHADOW_OUT);
  	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (rss_control));
	
	rss_control->details->control = (BonoboObject*) bonobo_control_new (GTK_WIDGET (frame));

	/* attach a property bag with the configure property */
	property_bag = bonobo_property_bag_new (get_bonobo_properties, set_bonobo_properties, rss_control);
	bonobo_control_set_properties (BONOBO_CONTROL(rss_control->details->control), property_bag);

	bonobo_property_bag_add (property_bag, "configuration", 1, BONOBO_ARG_STRING, NULL,
				 "RSS Configuration", 0);
	bonobo_object_unref (BONOBO_OBJECT (property_bag));
			 	
	/* show the view itself */	
	gtk_widget_show (GTK_WIDGET (rss_control));

	/* set up the rss file  (initially hardwired) */
	/*
	nautilus_rss_control_set_uri (rss_control, "http://www.slashdot.org/slashdot.rdf");
	*/
}

static void
nautilus_rss_control_clear_items (NautilusRSSControl *rss_control)
{
	if (rss_control->details->items != NULL) {
		nautilus_g_list_free_deep (rss_control->details->items);
		rss_control->details->items = NULL;	
	}
}

static void
nautilus_rss_control_destroy (GtkObject *object)
{
	NautilusRSSControl *rss_control;
	
	rss_control = NAUTILUS_RSS_CONTROL (object);
	g_free (rss_control->details->rss_uri);
	g_free (rss_control->details->title);
	
	if (rss_control->details->logo != NULL) {
		gdk_pixbuf_unref (rss_control->details->logo);
	}
	
	if (rss_control->details->bullet != NULL) {
		gdk_pixbuf_unref (rss_control->details->bullet);
	}

	if (rss_control->details->items != NULL) {
		nautilus_rss_control_clear_items (rss_control);
	}
	
	if (rss_control->details->font) {
		gtk_object_unref (GTK_OBJECT (rss_control->details->font));
	}
	
	g_free (rss_control->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* Component embedding support */
BonoboObject *
nautilus_rss_control_get_control (NautilusRSSControl *rss_control)
{
	return rss_control->details->control;
}

static void
nautilus_rss_control_set_title (NautilusRSSControl *rss_control, const char *title)
{
	if (nautilus_strcmp (rss_control->details->title, title) == 0) {
		return;
	}
	
	if (rss_control->details->title) {
		g_free (rss_control->details->title);
	}
	if (title != NULL) {
		rss_control->details->title = g_strdup (title);
	} else {
		rss_control->details->title = NULL;
	
	}
	gtk_widget_queue_draw (GTK_WIDGET (rss_control));
}


static void
rss_logo_callback (GnomeVFSResult  error, GdkPixbuf *pixbuf, gpointer callback_data)
{
	NautilusRSSControl *rss_control;
	
	rss_control = NAUTILUS_RSS_CONTROL (callback_data);
	if (rss_control->details->logo) {
		gdk_pixbuf_unref (rss_control->details->logo);
	}
	
	if (pixbuf != NULL) {
		gdk_pixbuf_ref (pixbuf);
		rss_control->details->logo = pixbuf;
		gtk_widget_queue_draw (GTK_WIDGET (rss_control));
	}
}

/* completion routine invoked when we've loaded the rss file uri.  Parse the xml document, and
 * then extract the various elements that we require */

static void
rss_read_done_callback (GnomeVFSResult result,
			 GnomeVFSFileSize file_size,
			 char *file_contents,
			 gpointer callback_data)
{
	xmlDocPtr rss_document;
	xmlNodePtr image_node, channel_node;
	xmlNodePtr  current_node, title_node, uri_node;
	char *image_uri, *title;
	NautilusRSSControl *rss_control;
	NautilusPixbufLoadHandle *load_image_handle;
	
	char *buffer;

	/* make sure the read was successful */
	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		return;
	}

	rss_control = NAUTILUS_RSS_CONTROL (callback_data);
	
	/* Parse the rss file with gnome-xml. The gnome-xml parser requires a zero-terminated array. */
	buffer = g_realloc (file_contents, file_size + 1);
	buffer[file_size] = '\0';
	rss_document = xmlParseMemory (buffer, file_size);
	g_free (buffer);

	/* extract the title and set it */
	channel_node = nautilus_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "channel");
	if (channel_node != NULL) {		
			title_node = nautilus_xml_get_child_by_name (channel_node, "title");
			if (title_node != NULL) {
				title = xmlNodeGetContent (title_node);				
				if (title != NULL) {
					nautilus_rss_control_set_title (rss_control, title);
					xmlFree (title);	
				}
			}
	}
		
	/* extract the image uri and, if found, load it asynchronously */
	image_node = nautilus_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "image");
	if (image_node != NULL) {		
		uri_node = nautilus_xml_get_child_by_name (image_node, "url");
		if (uri_node != NULL) {
			image_uri = xmlNodeGetContent (uri_node);
			if (image_uri != NULL) {
				load_image_handle = nautilus_gdk_pixbuf_load_async (image_uri, rss_logo_callback, rss_control);
				xmlFree (image_uri);
			}
		}
	}
			
	/* extract the items in a loop */
	nautilus_rss_control_clear_items (rss_control);
	current_node = rss_document->root->childs;
	while (current_node != NULL) {
		if (nautilus_strcmp (current_node->name, "item") == 0) {
			title_node = nautilus_xml_get_child_by_name (current_node, "title");
			if (title_node) {
				title = xmlNodeGetContent (title_node);
				rss_control->details->items = g_list_append (rss_control->details->items, g_strdup (title));
				xmlFree (title);
			}
		}
		current_node = current_node->next;
	}
		
	/* we're done, so free everything up */
	xmlFreeDoc (rss_document);
	
	/* schedule a redraw to reflect the new contents */
	gtk_widget_queue_draw (GTK_WIDGET (rss_control));
}

/* load the rs file asynchronously */
static void
load_rss_file (NautilusRSSControl *rss_control)
{
	char *title;
	/* load the uri asynchrounously, calling a completion routine when completed */
	nautilus_read_entire_file_async (rss_control->details->rss_uri, rss_read_done_callback, rss_control);
	
	/* put up a title that's displayed while we wait */
	title = g_strdup_printf ("Loading %s", rss_control->details->rss_uri);
	nautilus_rss_control_set_title (rss_control, title);
	g_free (title);
}

/* set the uri and load it */
static void
nautilus_rss_control_set_uri (NautilusRSSControl *rss_control, const char *uri)
{

	if (nautilus_strcmp (rss_control->details->rss_uri, uri) == 0) {
		return;
	}
	
	if (rss_control->details->rss_uri != NULL) {
		g_free (rss_control->details->rss_uri);
		rss_control->details->rss_uri = NULL;
	}

	if (uri != NULL) {
		rss_control->details->rss_uri = g_strdup (uri);
		load_rss_file (rss_control);	
	}
}



/* draw the logo image */
static int
draw_rss_logo_image (NautilusRSSControl *rss_control, GdkPixbuf *pixbuf, int offset)
{
	GtkWidget *widget;
	int logo_width, logo_height;
	int v_offset;
	
	widget = GTK_WIDGET (rss_control);
	v_offset = offset;
	
	if (rss_control->details->logo != NULL) {
		logo_width = gdk_pixbuf_get_width (rss_control->details->logo);
		logo_height = gdk_pixbuf_get_height (rss_control->details->logo);

		gdk_pixbuf_composite (rss_control->details->logo, pixbuf, 2, v_offset, logo_width, logo_height,
					2, v_offset, 1.0, 1.0, GDK_PIXBUF_ALPHA_BILEVEL, 255);
		v_offset += logo_height + 2;
	}

	return v_offset;
}

/* draw the title */
static int
draw_rss_title (NautilusRSSControl *rss_control, GdkPixbuf *pixbuf, int v_offset)
{
	int title_width, title_height;
	GtkWidget *widget;
	
	if (rss_control->details->title == NULL || rss_control->details->font == NULL) {
		return v_offset;
	}
	
	widget = GTK_WIDGET (rss_control);

	/* first, measure the text */
	nautilus_scalable_font_measure_text (rss_control->details->font, 
					     18, 18,
					     rss_control->details->title, strlen (rss_control->details->title),
					     &title_width, 
					     &title_height);
	
	/* draw the name into the pixbuf using anti-aliased text */	
	nautilus_scalable_font_draw_text (rss_control->details->font, pixbuf, 
					  4, v_offset,
					  NULL,
					  18, 18,
					  rss_control->details->title, strlen (rss_control->details->title),
					  NAUTILUS_RGB_COLOR_BLACK,
					  NAUTILUS_OPACITY_NONE);

	return v_offset + title_height;
}

/* utility for underlining an item - assumes the pixbuf has an alpha channel */
static void
draw_blue_line (GdkPixbuf *pixbuf, int x, int y, int width)
{
	guchar *pixels_ptr;
	int row_stride, line_width, pixbuf_width, i;
	
	line_width = width;
	pixbuf_width = gdk_pixbuf_get_width (pixbuf);
	if ((x + line_width) > pixbuf_width) {
		line_width = pixbuf_width - x - 1;
	}
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels_ptr = gdk_pixbuf_get_pixels (pixbuf);

	pixels_ptr += (4 * x) + (row_stride * y);
	for (i = 0; i < line_width; i++) {
		*pixels_ptr++ = 0;
		*pixels_ptr++ = 0;
		*pixels_ptr++ = 159;
		*pixels_ptr++ = 255;
		
	}
}

/* draw the items */
static int
draw_rss_items (NautilusRSSControl *rss_control, GdkPixbuf *pixbuf, int v_offset)
{
	GList *current_item;
	int bullet_width, bullet_height;
	int text_width, text_height, maximum_height;

	maximum_height = GTK_WIDGET (rss_control)->allocation.height - 16;
		
	if (rss_control->details->bullet) {
		bullet_width = gdk_pixbuf_get_width (rss_control->details->bullet);
		bullet_height = gdk_pixbuf_get_height (rss_control->details->bullet);
	} else {
		bullet_width = 0;
		bullet_height = 0;
	}
	
	current_item = rss_control->details->items;

	while (current_item != NULL) {		
		/* draw the text */

		nautilus_scalable_font_measure_text (rss_control->details->font, 
					     12, 12,
					     current_item->data, strlen (current_item->data),
					     &text_width, 
					     &text_height);
		
		nautilus_scalable_font_draw_text (rss_control->details->font, pixbuf, 
					  20, v_offset,
					  NULL,
					  12, 12,
					  current_item->data, strlen (current_item->data),
					  NAUTILUS_RGB_COLOR_BLACK,
					  NAUTILUS_OPACITY_NONE);

		/* draw a blue underline to make it look like a link */
		draw_blue_line (pixbuf, 20, v_offset + 11, text_width);
		
		/* draw the bullet */	
		if (rss_control->details->bullet) {
			gdk_pixbuf_composite (rss_control->details->bullet, pixbuf, 2, v_offset - 2, bullet_width, bullet_height,
					2, v_offset - 2, 1.0, 1.0, GDK_PIXBUF_ALPHA_BILEVEL, 192);
		}
		
		v_offset += 15;
		current_item = current_item->next;
		if (v_offset > maximum_height) {
			break;
		}

	}
	
	return v_offset; 
}

/* handle drawing the control */
static void
nautilus_rss_control_draw (GtkWidget *widget, GdkRectangle *box)
{
	NautilusRSSControl *control;
	GdkPixbuf *temp_pixbuf;
	int width, height, v_offset;

	/* allocate a pixbuf to draw into */
	width = widget->allocation.width;
	height = widget->allocation.height;
	
	temp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
	nautilus_gdk_pixbuf_fill_rectangle_with_color (temp_pixbuf, NULL, 0xFFEFEFEF);
		
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_RSS_CONTROL (widget));

	control = NAUTILUS_RSS_CONTROL (widget);

	v_offset = draw_rss_logo_image (control, temp_pixbuf, 2);
	v_offset = draw_rss_title (control, temp_pixbuf, v_offset);
	v_offset += 6;
	v_offset = draw_rss_items (control, temp_pixbuf, v_offset);

	/* blit the pixbuf to the drawable, then release it */
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
nautilus_rss_control_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GdkRectangle box;
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_RSS_CONTROL (widget), FALSE);

	box.x = 0; box.y = 0;
	box.width = widget->allocation.width;
	box.height = widget->allocation.height;

	nautilus_rss_control_draw (widget, &box);
	return FALSE;
}

/* handle button press events */
static gboolean
nautilus_rss_control_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	g_message ("button press");
	return FALSE;
}

