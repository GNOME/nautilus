/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the zoom control for the location bar
 *
 */

#include <config.h>
#include "nautilus-zoom-control.h"

#include <libgnome/gnome-defs.h>

#include <math.h>
#include <gnome.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-theme.h>

enum {
	ZOOM_IN,
	ZOOM_OUT,
	ZOOM_TO_LEVEL,
	ZOOM_TO_FIT,
	LAST_SIGNAL
};

#define	GAP_WIDTH 2

struct NautilusZoomControlDetails {
	double zoom_level;
	double min_zoom_level;	 
	double max_zoom_level;
	GList *preferred_zoom_levels;

	int y_offset;
	GdkPixbuf *zoom_body_image;
	GdkPixbuf *zoom_decrement_image;
	GdkPixbuf *zoom_increment_image;
	GdkPixbuf *number_strip;
};

static guint signals[LAST_SIGNAL];

static void     nautilus_zoom_control_initialize_class 	 (NautilusZoomControlClass *klass);
static void     nautilus_zoom_control_initialize       	 (NautilusZoomControl *zoom_control);
static void	nautilus_zoom_control_destroy		 (GtkObject *object);
static void     nautilus_zoom_control_draw 	       	 (GtkWidget *widget, 
							  GdkRectangle *box);
static int     nautilus_zoom_control_expose 		 (GtkWidget *widget, 
							  GdkEventExpose *event);
static gboolean nautilus_zoom_control_button_press_event (GtkWidget *widget, 
							  GdkEventButton *event);
static void	nautilus_zoom_control_load_images	 (NautilusZoomControl *zoom_control);
static void	nautilus_zoom_control_unload_images	 (NautilusZoomControl *zoom_control);
static void	nautilus_zoom_control_theme_changed 	 (gpointer user_data);
static void	nautilus_zoom_control_size_allocate	 (GtkWidget *widget, GtkAllocation *allocation);


void            draw_number		                 (GtkWidget *widget, 
							  GdkRectangle *box);

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusZoomControl, nautilus_zoom_control, GTK_TYPE_EVENT_BOX)

static void
nautilus_zoom_control_initialize_class (NautilusZoomControlClass *zoom_control_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (zoom_control_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (zoom_control_class);
	
	object_class->destroy = nautilus_zoom_control_destroy;

	widget_class->draw = nautilus_zoom_control_draw;
	widget_class->expose_event = nautilus_zoom_control_expose;
	widget_class->button_press_event = nautilus_zoom_control_button_press_event;
	widget_class->size_allocate = nautilus_zoom_control_size_allocate;
	
	signals[ZOOM_IN] =
		gtk_signal_new ("zoom_in",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusZoomControlClass, 
						   zoom_in),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	signals[ZOOM_OUT] =
		gtk_signal_new ("zoom_out",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusZoomControlClass, 
						   zoom_out),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	signals[ZOOM_TO_LEVEL] =
		gtk_signal_new ("zoom_to_level",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusZoomControlClass, 
						   zoom_to_level),
				nautilus_gtk_marshal_NONE__DOUBLE,
				GTK_TYPE_NONE,
				1,
				GTK_TYPE_DOUBLE);

	signals[ZOOM_TO_FIT] =
		gtk_signal_new ("zoom_to_fit",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusZoomControlClass, 
						   zoom_to_fit),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void 
nautilus_zoom_control_destroy (GtkObject *object)
{
	nautilus_g_list_free_deep (NAUTILUS_ZOOM_CONTROL (object)->details->preferred_zoom_levels);
	NAUTILUS_ZOOM_CONTROL (object)->details->preferred_zoom_levels = NULL;

	/* deallocate pixbufs */
	nautilus_zoom_control_unload_images (NAUTILUS_ZOOM_CONTROL (object));

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_zoom_control_theme_changed,
					      object);

	g_free (NAUTILUS_ZOOM_CONTROL (object)->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static int
get_zoom_width (NautilusZoomControl *zoom_control)
{
	int total_width;
	total_width = gdk_pixbuf_get_width (zoom_control->details->zoom_increment_image);
	total_width += GAP_WIDTH;
	total_width += gdk_pixbuf_get_width (zoom_control->details->zoom_body_image);
	total_width += GAP_WIDTH;
	total_width += gdk_pixbuf_get_width (zoom_control->details->zoom_decrement_image);
	return total_width;
}

static void
nautilus_zoom_control_initialize (NautilusZoomControl *zoom_control)
{
	GtkWidget *widget = GTK_WIDGET (zoom_control);
	int	  zoom_width;

	GTK_WIDGET_SET_FLAGS (zoom_control, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (zoom_control, GTK_NO_WINDOW);

	gtk_widget_set_events (widget, 
			       gtk_widget_get_events (widget) | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	
	zoom_control->details = g_new0 (NautilusZoomControlDetails, 1);
	
	zoom_control->details->zoom_level = 1.0;
	zoom_control->details->min_zoom_level = 0.0;
	zoom_control->details->max_zoom_level = 2.0;
	zoom_control->details->preferred_zoom_levels = NULL;

	/* allocate the pixmap that holds the image */
	nautilus_zoom_control_load_images (zoom_control);
	
	zoom_width = get_zoom_width (zoom_control);
	gtk_widget_set_usize (GTK_WIDGET (zoom_control), zoom_width, -1);

	/* add a callback for when the theme changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
					  nautilus_zoom_control_theme_changed,
					  zoom_control);	

}

/* allocate a new zoom control */
GtkWidget*
nautilus_zoom_control_new ()
{
	NautilusZoomControl *zoom_control = gtk_type_new (nautilus_zoom_control_get_type ());
	return GTK_WIDGET (zoom_control);
}

/* handler for handling theme changes */
static void
nautilus_zoom_control_theme_changed (gpointer user_data)
{
	NautilusZoomControl *zoom_control;

	zoom_control = NAUTILUS_ZOOM_CONTROL (user_data);
	gtk_widget_hide (GTK_WIDGET (zoom_control));
	nautilus_zoom_control_load_images (zoom_control);
	gtk_widget_show (GTK_WIDGET (zoom_control));	
}

static int
get_zoom_offset (const char *property)
{
	char *num_str;
	int result;
	
	result = 0;
	num_str = nautilus_theme_get_theme_data ("zoom_control", property);
	
	if (num_str) {
		result = atoi (num_str);
		g_free (num_str);
	}

	return result;
}

/* draw the current zoom percentage, using a custom number strip if one is available, or
   using the ordinary text routines otherwise */
void draw_number (GtkWidget *widget, GdkRectangle *box)
{
	char buffer[8];
	char *cur_char;
	GdkFont *label_font;
	GdkGC* temp_gc; 
	int x, y, percent; 
	int char_height, char_width, char_offset;
	int num_v_offset, num_h_offset;
	NautilusZoomControl *zoom_control;
	
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	
	num_v_offset = get_zoom_offset ("NUMBER_V_OFFSET");
	num_h_offset = get_zoom_offset ("NUMBER_H_OFFSET");
	char_width = get_zoom_offset ("DIGIT_WIDTH");
	if (char_width == 0)
		char_width = 6;
		
	percent = floor((100.0 * zoom_control->details->zoom_level) + .5);
	g_snprintf(buffer, 8, "%d", percent);

	y = 1 + num_v_offset + (box->height >> 1);
	x = num_h_offset + ((box->width - char_width * strlen(buffer)) >> 1);  
	
	temp_gc = gdk_gc_new(widget->window);
	
	if (zoom_control->details->number_strip) {
		cur_char = &buffer[0];
		char_height = gdk_pixbuf_get_height (zoom_control->details->number_strip);
		while (*cur_char) {
			/* draw the character */
			
			char_offset = (*cur_char++ - '0') * char_width;
			gdk_pixbuf_render_to_drawable_alpha (zoom_control->details->number_strip,
					     widget->window, 
					     char_offset, 0, x, y,
					     char_width,
					     char_height,
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);

			x += char_width;
		}
	} else {
		label_font = gdk_font_load("-bitstream-courier-medium-r-normal-*-9-*-*-*-*-*-*-*");
				
		x = num_h_offset + ((box->width - gdk_string_width(label_font, buffer)) >> 1);  
		
		gdk_draw_string (widget->window, label_font, temp_gc, x, y, &buffer[0]);
		gdk_font_unref(label_font);
	}
	
	gdk_gc_unref(temp_gc);
}

/* utility to simplify drawing */
static void
draw_pixbuf (GdkPixbuf *pixbuf, GdkDrawable *drawable, int x, int y)
{
	gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable, 0, 0, x, y,
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf),
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);
}

/* draw the zoom control image into the passed-in rectangle */

static void
draw_zoom_control_image (GtkWidget *widget, GdkRectangle *box)
{
	NautilusZoomControl *zoom_control;
	int offset, width, height;
		
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	/* draw the decrement symbol if necessary */
	if (zoom_control->details->zoom_level > zoom_control->details->min_zoom_level) {
		draw_pixbuf (zoom_control->details->zoom_decrement_image, widget->window, box->x, box->y + zoom_control->details->y_offset);
	} else {
		width  = gdk_pixbuf_get_width (zoom_control->details->zoom_decrement_image);
		height = gdk_pixbuf_get_height (zoom_control->details->zoom_decrement_image);		
		
		/* Clear the symbol area to get the default widget background */
		gdk_window_clear_area (widget->window,
				       box->x,
				       box->y + zoom_control->details->y_offset,
				       width,
				       height);
	}
	
	offset = gdk_pixbuf_get_width (zoom_control->details->zoom_decrement_image) + GAP_WIDTH;
	/* draw the body image */
	draw_pixbuf (zoom_control->details->zoom_body_image, widget->window, box->x + offset, box->y + zoom_control->details->y_offset);
	offset += gdk_pixbuf_get_width (zoom_control->details->zoom_body_image) + GAP_WIDTH;
	
	/* draw the increment symbol if necessary */
	if (zoom_control->details->zoom_level < zoom_control->details->max_zoom_level) {
		draw_pixbuf (zoom_control->details->zoom_increment_image, widget->window, box->x + offset, box->y + zoom_control->details->y_offset);
	} else {
		width  = gdk_pixbuf_get_width (zoom_control->details->zoom_increment_image);
		height = gdk_pixbuf_get_height (zoom_control->details->zoom_increment_image);		

		/* Clear the symbol area to get the default widget background */
		gdk_window_clear_area (widget->window,
				       box->x + offset,
				       box->y + zoom_control->details->y_offset,
				       width,
				       height);
	}
}

static void
nautilus_zoom_control_draw (GtkWidget *widget, GdkRectangle *box)
{ 
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_ZOOM_CONTROL (widget));

	/* Clear the widget get the default widget background before drawing our stuff */
	gdk_window_clear_area (widget->window,
			       0,
			       0,
			       widget->allocation.width,
			       widget->allocation.height);

	draw_zoom_control_image (widget, box);	
	draw_number (widget, box);
}

/* handle expose events */

static int
nautilus_zoom_control_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GdkRectangle box;
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_ZOOM_CONTROL (widget), FALSE);

	box.x = 0; box.y = 0;
	box.width = widget->allocation.width;
	box.height = widget->allocation.height;

	draw_zoom_control_image (widget, &box);	
	draw_number (widget, &box);
	
	return FALSE;
}


/* routines to load the images used to draw the control */

/* load or reload the images from the current theme */

static void
nautilus_zoom_control_unload_images (NautilusZoomControl *zoom_control)
{
	if (zoom_control->details->zoom_body_image) {
		gdk_pixbuf_unref (zoom_control->details->zoom_body_image);
	}
	
	if (zoom_control->details->zoom_decrement_image) {
		gdk_pixbuf_unref (zoom_control->details->zoom_decrement_image);
	}
	
	if (zoom_control->details->zoom_increment_image) {
		gdk_pixbuf_unref (zoom_control->details->zoom_increment_image);
	}

}

static GdkPixbuf*
load_themed_image (const char *file_name)
{
	GdkPixbuf *pixbuf;
	char *image_path;
	
	image_path = nautilus_theme_get_image_path (file_name);
	if (image_path) {
		pixbuf = gdk_pixbuf_new_from_file (image_path);
		g_free (image_path);
		return pixbuf;
	}
	return NULL;
}

static void
nautilus_zoom_control_update_offsets (NautilusZoomControl *zoom_control)
{
	zoom_control->details->y_offset = (GTK_WIDGET (zoom_control)->allocation.height - gdk_pixbuf_get_height (zoom_control->details->zoom_body_image)) >> 1;
}

static void
nautilus_zoom_control_load_images (NautilusZoomControl *zoom_control)
{
	nautilus_zoom_control_unload_images (zoom_control);

	zoom_control->details->zoom_body_image = load_themed_image ("zoom_body.png");
	zoom_control->details->zoom_decrement_image = load_themed_image ("decrement.png");
	zoom_control->details->zoom_increment_image = load_themed_image ("increment.png");
	zoom_control->details->number_strip = load_themed_image ("number_strip.png");

	nautilus_zoom_control_update_offsets (zoom_control);
}

/* routines to create and handle the zoom menu */

static void
zoom_menu_callback (GtkMenuItem *item, gpointer callback_data)
{
	double zoom_level;
	NautilusZoomControl *zoom_control;
		
	zoom_level = *(double*) gtk_object_get_data (GTK_OBJECT (item), "zoom_level");
	zoom_control = NAUTILUS_ZOOM_CONTROL (callback_data);
	
	/* Check to see if we can zoom out */
	if ((zoom_control->details->min_zoom_level <= zoom_level && zoom_level <  zoom_control->details->zoom_level) ||
	    (zoom_control->details->zoom_level     < zoom_level && zoom_level  <= zoom_control->details->max_zoom_level)) {
		gtk_signal_emit (GTK_OBJECT (zoom_control), signals[ZOOM_TO_LEVEL], zoom_level);
	}
}

static void
create_zoom_menu_item (GtkMenu *menu, GtkWidget *zoom_control, double zoom_level)
{
	GtkWidget *menu_item;
	double	  *zoom_level_ptr;
	char	  item_text[8];
	
	g_snprintf(item_text, sizeof (item_text), _("%.0f%%"), 100.0 * zoom_level);

	menu_item = gtk_menu_item_new_with_label (item_text);

	zoom_level_ptr = g_new (double, 1);
	*zoom_level_ptr = zoom_level;

	gtk_object_set_data_full (GTK_OBJECT (menu_item), "zoom_level", zoom_level_ptr, g_free);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (zoom_menu_callback),
			    zoom_control);

  	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);
}

static GtkMenu*
create_zoom_menu(GtkWidget *zoom_control)
{
	GList *p;
	GtkMenu *menu;

	menu = GTK_MENU (gtk_menu_new ());

	p = NAUTILUS_ZOOM_CONTROL (zoom_control)->details->preferred_zoom_levels;
	
	while (p != NULL) {
		create_zoom_menu_item(menu, zoom_control,  *(double*)p->data);
		p = g_list_next (p);
	}
	
	return menu;  
}
  
/* handle button presses */

static gboolean
nautilus_zoom_control_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	int width = widget->allocation.width;
	int center = width >> 1;
	
	/* check for the context menu button and handle by creating and showing the menu */  
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		GtkMenu *zoom_menu = create_zoom_menu (widget);
		
		gtk_object_ref (GTK_OBJECT (zoom_menu));
		gtk_object_sink (GTK_OBJECT (zoom_menu));
		
		gtk_menu_popup (zoom_menu, NULL, NULL, NULL, NULL, 3, GDK_CURRENT_TIME);
		gtk_object_unref (GTK_OBJECT (zoom_menu));
		return TRUE;	  
 	}
	
	if (event->x < (width / 3) && (zoom_control->details->zoom_level > zoom_control->details->min_zoom_level)) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_OUT]);			
	} else if ((event->x > ((2 * width) / 3)) && (zoom_control->details->zoom_level < zoom_control->details->max_zoom_level)) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_IN]);			
	} else if ((event->x >= (center - (width >> 3))) && (event->x <= (center + (width >> 3)))) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_TO_FIT]);			
	}

	/*
	 * We don't change our state (to reflect the new zoom) here. The zoomable will
	 * call back with the new level. Actually, the callback goes to the view-frame
	 * containing the zoomable which, in turn, emits zoom_level_changed, which
	 * someone (e.g. nautilus_window) picks up and handles by calling into us -
	 * nautilus_zoom_control_set_zoom_level.
	 */	  
  
	return TRUE;
}

/* handle setting the size */
static void
nautilus_zoom_control_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
	widget->allocation.width = get_zoom_width (zoom_control);
   	widget->allocation.height = allocation->height;
	nautilus_zoom_control_update_offsets (zoom_control);
}

void
nautilus_zoom_control_set_zoom_level (NautilusZoomControl *zoom_control, double zoom_level)
{
	zoom_control->details->zoom_level = zoom_level;
	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
}

void
nautilus_zoom_control_set_min_zoom_level (NautilusZoomControl *zoom_control, double  zoom_level)
{
	zoom_control->details->min_zoom_level = zoom_level;
	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
}

void
nautilus_zoom_control_set_max_zoom_level (NautilusZoomControl *zoom_control, double zoom_level)
{
	zoom_control->details->max_zoom_level = zoom_level;
	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
}

void
nautilus_zoom_control_set_preferred_zoom_levels (NautilusZoomControl *zoom_control, GList* zoom_levels)
{
	nautilus_g_list_free_deep (zoom_control->details->preferred_zoom_levels);
	zoom_control->details->preferred_zoom_levels = zoom_levels;
}

double
nautilus_zoom_control_get_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->details->zoom_level;
}

double
nautilus_zoom_control_get_min_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->details->min_zoom_level;
}

double
nautilus_zoom_control_get_max_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->details->max_zoom_level;
}


