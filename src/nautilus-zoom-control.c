/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
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

#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-extensions.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkradiomenuitem.h>
#include <libgnome/gnome-macros.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-marshal.h>
#include <libnautilus-private/nautilus-theme.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
	ZOOM_IN,
	ZOOM_OUT,
	ZOOM_TO_LEVEL,
	ZOOM_TO_FIT,
	LAST_SIGNAL
};

typedef enum {
	PRELIGHT_NONE,
	PRELIGHT_MINUS,
	PRELIGHT_CENTER,
	PRELIGHT_PLUS
} PrelightMode;

#define	GAP_WIDTH 2

struct NautilusZoomControlDetails {
	double zoom_level;
	double min_zoom_level;	 
	double max_zoom_level;
	gboolean has_min_zoom_level;
	gboolean has_max_zoom_level;
	GList *preferred_zoom_levels;

	int y_offset;
	GdkPixbuf *zoom_body_image;
	GdkPixbuf *zoom_decrement_image;
	GdkPixbuf *zoom_increment_image;
	GdkPixbuf *number_strip;
	PrelightMode prelight_mode;

	PangoLayout *layout;

	gboolean marking_menu_items;
};


static guint signals[LAST_SIGNAL];

static void nautilus_zoom_control_load_images   (NautilusZoomControl      *zoom_control);
static void nautilus_zoom_control_unload_images (NautilusZoomControl      *zoom_control);
static void nautilus_zoom_control_theme_changed (gpointer                  user_data);

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

GNOME_CLASS_BOILERPLATE (NautilusZoomControl, nautilus_zoom_control,
			 GtkEventBox, GTK_TYPE_EVENT_BOX)

static void
nautilus_zoom_control_finalize (GObject *object)
{
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					 nautilus_zoom_control_theme_changed,
					 object);

	nautilus_zoom_control_unload_images (NAUTILUS_ZOOM_CONTROL (object));

	eel_g_list_free_deep (NAUTILUS_ZOOM_CONTROL (object)->details->preferred_zoom_levels);
	g_free (NAUTILUS_ZOOM_CONTROL (object)->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
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
nautilus_zoom_control_instance_init (NautilusZoomControl *zoom_control)
{
	GTK_WIDGET_SET_FLAGS (zoom_control, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (zoom_control, GTK_NO_WINDOW);

	gtk_widget_add_events (GTK_WIDGET (zoom_control), 
			       GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK);
	
	zoom_control->details = g_new0 (NautilusZoomControlDetails, 1);
	
	zoom_control->details->zoom_level = 1.0;
	zoom_control->details->min_zoom_level = 0.0;
	zoom_control->details->max_zoom_level = 2.0;
	zoom_control->details->has_min_zoom_level = TRUE;
	zoom_control->details->has_max_zoom_level = TRUE;

	nautilus_zoom_control_load_images (zoom_control);
	
	gtk_widget_set_size_request (GTK_WIDGET (zoom_control),
				     get_zoom_width (zoom_control), -1);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
				      nautilus_zoom_control_theme_changed,
				      zoom_control);
}

/* allocate a new zoom control */
GtkWidget *
nautilus_zoom_control_new (void)
{
	return gtk_widget_new (nautilus_zoom_control_get_type (), NULL);
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
	if (num_str != NULL) {
		result = atoi (num_str);
		g_free (num_str);
	}

	return result;
}

/* draw the current zoom percentage, using a custom number strip if one is available, or
   using the ordinary text routines otherwise */
static void
draw_number (GtkWidget *widget, GdkRectangle *box)
{
	char buffer[8];
	char *cur_char;
	int x, y, percent; 
	int char_height, char_width, char_offset;
	int num_v_offset, num_h_offset;
	NautilusZoomControl *zoom_control;
	GdkPixbuf *number_pixbuf;
	PangoRectangle logical_rect;

	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	
	num_v_offset = get_zoom_offset ("number_v_offset");
	num_h_offset = get_zoom_offset ("number_h_offset");
		
	percent = floor ((100.0 * zoom_control->details->zoom_level) + .5);
	g_snprintf (buffer, 8, "%d", percent);
	
	if (zoom_control->details->number_strip != NULL) {
		number_pixbuf = zoom_control->details->number_strip;
		if (zoom_control->details->prelight_mode == PRELIGHT_CENTER) {
			number_pixbuf = eel_create_spotlight_pixbuf (number_pixbuf);
		}
		
		char_width = get_zoom_offset ("digit_width");
		if (char_width <= 0) {
			char_width = gdk_pixbuf_get_width (number_pixbuf) / 10;
		}
		char_height = gdk_pixbuf_get_height (number_pixbuf);

		x = box->x + num_h_offset + ((box->width - char_width * strlen (buffer)) >> 1);  
		y = box->y + num_v_offset + ((box->height - char_height) >> 1);

		cur_char = &buffer[0];		
		while (*cur_char) {
			char_offset = (*cur_char++ - '0') * char_width;
			gdk_pixbuf_render_to_drawable_alpha (number_pixbuf,
							     widget->window, 
							     char_offset, 0, x, y,
							     char_width,
							     char_height,
							     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
							     0, 0);
			x += char_width;
		}
	
		if (number_pixbuf != zoom_control->details->number_strip) {
			g_object_unref (number_pixbuf);
		}
	} else {
		pango_layout_set_text (zoom_control->details->layout, buffer, -1);
		pango_layout_get_pixel_extents (zoom_control->details->layout, NULL, &logical_rect);

		x = box->x + num_h_offset + (box->width - logical_rect.width)  / 2;
		y = box->y + num_v_offset + (box->height - logical_rect.height) / 2;

		gtk_paint_layout (widget->style,
				  widget->window,
				  GTK_WIDGET_STATE (widget),
				  FALSE,
				  box,
				  widget,
				  "zoom_control",
				  x, y,
				  zoom_control->details->layout);
	}
}

/* utilities to simplify drawing */
static void
draw_pixbuf (GdkPixbuf *pixbuf, GdkDrawable *drawable, int x, int y)
{
	gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable, 0, 0, x, y,
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf),
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);
}

static void
draw_pixbuf_with_prelight (NautilusZoomControl *zoom_control, GdkPixbuf *pixbuf, int x_pos, int y_pos, PrelightMode mode)
{
	GdkPixbuf *temp_pixbuf;
	temp_pixbuf = pixbuf;
	if (zoom_control->details->prelight_mode == mode) {
		temp_pixbuf = eel_create_spotlight_pixbuf (temp_pixbuf);
	}
	draw_pixbuf (temp_pixbuf, GTK_WIDGET (zoom_control)->window, x_pos, y_pos + zoom_control->details->y_offset);
	if (pixbuf != temp_pixbuf) {
		g_object_unref (temp_pixbuf);
	}

}

/* draw the zoom control image into the passed-in rectangle */

static void
draw_zoom_control_image (GtkWidget *widget, GdkRectangle *box)
{
	NautilusZoomControl *zoom_control;
	int offset, width, height;
		
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	/* draw the decrement symbol if necessary, complete with prelighting */
	if (nautilus_zoom_control_can_zoom_out (zoom_control)) {
		draw_pixbuf_with_prelight (zoom_control, zoom_control->details->zoom_decrement_image,
					   box->x, box->y, PRELIGHT_MINUS);
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
	/* draw the body image, prelighting if necessary */
	draw_pixbuf_with_prelight (zoom_control, zoom_control->details->zoom_body_image,
				   box->x + offset, box->y, PRELIGHT_CENTER);
	
	offset += gdk_pixbuf_get_width (zoom_control->details->zoom_body_image) + GAP_WIDTH;
	
	/* draw the increment symbol if necessary, complete with prelighting */
	if (nautilus_zoom_control_can_zoom_in (zoom_control)) {
		draw_pixbuf_with_prelight (zoom_control, zoom_control->details->zoom_increment_image,
					   box->x + offset, box->y, PRELIGHT_PLUS);
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

/* set the prelight mode and redraw as necessary */
static void
nautilus_zoom_control_set_prelight_mode (NautilusZoomControl *zoom_control, PrelightMode mode)
{
	if (mode != zoom_control->details->prelight_mode) {
		zoom_control->details->prelight_mode = mode;		
		gtk_widget_queue_draw (GTK_WIDGET (zoom_control));	
	}
	
}

/* routines to load the images used to draw the control */

/* load or reload the images from the current theme */

static void
nautilus_zoom_control_unload_images (NautilusZoomControl *zoom_control)
{
	if (zoom_control->details->zoom_body_image) {
		g_object_unref (zoom_control->details->zoom_body_image);
		zoom_control->details->zoom_body_image = NULL;
	}
	
	if (zoom_control->details->zoom_decrement_image) {
		g_object_unref (zoom_control->details->zoom_decrement_image);
		zoom_control->details->zoom_decrement_image = NULL;
	}
	
	if (zoom_control->details->zoom_increment_image) {
		g_object_unref (zoom_control->details->zoom_increment_image);
		zoom_control->details->zoom_increment_image = NULL;
	}

	if (zoom_control->details->number_strip != NULL) {
		g_object_unref (zoom_control->details->number_strip);
		zoom_control->details->number_strip = NULL;
	}

	if (zoom_control->details->layout != NULL) {
		g_object_unref (zoom_control->details->layout);
		zoom_control->details->layout = NULL;
	}
}

static GdkPixbuf*
load_themed_image (const char *file_name)
{
	GdkPixbuf *pixbuf;
	char *image_path;
	
	image_path = nautilus_theme_get_image_path (file_name);
	if (image_path) {
		pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
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
	PangoContext *context;
	PangoFontDescription *font_desc;
	char *font_name;

	nautilus_zoom_control_unload_images (zoom_control);

	zoom_control->details->zoom_body_image = load_themed_image ("zoom_body.png");
	zoom_control->details->zoom_decrement_image = load_themed_image ("decrement.png");
	zoom_control->details->zoom_increment_image = load_themed_image ("increment.png");

	zoom_control->details->number_strip = load_themed_image ("number_strip.png");

	if (zoom_control->details->number_strip == NULL) {
		context = gtk_widget_get_pango_context (GTK_WIDGET (zoom_control));
		zoom_control->details->layout = pango_layout_new (context);
		
		font_name = nautilus_theme_get_theme_data ("zoom", "font_name");
		if (font_name == NULL) {
			font_name = g_strdup ("Mono 9");
		}

		font_desc = pango_font_description_from_string (font_name);
		if (font_desc != NULL) {
			pango_layout_set_font_description (zoom_control->details->layout,
							   font_desc);
		}

		pango_font_description_free (font_desc);
		g_free (font_name);
	}

	nautilus_zoom_control_update_offsets (zoom_control);
}

/* routines to create and handle the zoom menu */

static void
zoom_menu_callback (GtkMenuItem *item, gpointer callback_data)
{
	double zoom_level;
	NautilusZoomControl *zoom_control;
	gboolean can_zoom;
		
	zoom_control = NAUTILUS_ZOOM_CONTROL (callback_data);

	/* Don't do anything if we're just setting the toggle state of menu items. */
	if (zoom_control->details->marking_menu_items) {
		return;
	}

	zoom_level = * (double *) g_object_get_data (G_OBJECT (item), "zoom_level");

	/* Assume we can zoom and then check whether we're right. */
	can_zoom = TRUE;
	if (zoom_control->details->has_min_zoom_level &&
	    zoom_level < zoom_control->details->min_zoom_level)
		can_zoom = FALSE; /* no, we're below the minimum zoom level. */
	if (zoom_control->details->has_max_zoom_level &&
	    zoom_level > zoom_control->details->max_zoom_level)
		can_zoom = FALSE; /* no, we're beyond the upper zoom level. */

	/* if we can zoom */
	if (can_zoom) {	
		g_signal_emit (zoom_control, signals[ZOOM_TO_LEVEL], 0, zoom_level);
	}
}

static GtkRadioMenuItem *
create_zoom_menu_item (NautilusZoomControl *zoom_control, GtkMenu *menu,
		       float zoom_level,
		       GtkRadioMenuItem *previous_radio_item)
{
	GtkWidget *menu_item;
	char *item_text;
	double *zoom_level_ptr;
	GSList *radio_item_group;
	int percent;
	
	/* Set flag so that callback isn't activated when set_active called
	 * to set toggle state of other radio items.
	 */
	zoom_control->details->marking_menu_items = TRUE;

	/* This is marked for localization in case the % sign is not
	 * appropriate in some locale. I guess that's unlikely.
	 */
	percent = floor ((100.0 * zoom_level) + .5);
	item_text = g_strdup_printf ("%d%%", percent);

	radio_item_group = previous_radio_item == NULL
		? NULL
		: gtk_radio_menu_item_get_group (previous_radio_item);
	menu_item = gtk_radio_menu_item_new_with_label (radio_item_group, item_text);

	zoom_level_ptr = g_new (double, 1);
	*zoom_level_ptr = zoom_level;

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), 
					zoom_level == zoom_control->details->zoom_level);
	
	g_object_set_data_full (G_OBJECT (menu_item), "zoom_level", zoom_level_ptr, g_free);
	g_signal_connect_object (menu_item, "activate",
				 G_CALLBACK (zoom_menu_callback), zoom_control, 0);

  	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	zoom_control->details->marking_menu_items = FALSE;

	return GTK_RADIO_MENU_ITEM (menu_item);
}

static GtkMenu *
create_zoom_menu (NautilusZoomControl *zoom_control)
{
	GtkMenu *menu;
	GtkRadioMenuItem *previous_item;
	GList *node;

	menu = GTK_MENU (gtk_menu_new ());

	previous_item = NULL;
	for (node = zoom_control->details->preferred_zoom_levels; node != NULL; node = node->next) {
		previous_item = create_zoom_menu_item
			(zoom_control, menu, * (float *) node->data, previous_item);
	}
	
	return menu;  
}
  
/* handle button presses */

static gboolean
nautilus_zoom_control_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	NautilusZoomControl *zoom_control;
	int width, center;
	
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	/* check for the context menu button and handle by creating and showing the menu */  
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		eel_pop_up_context_menu (create_zoom_menu (zoom_control), 
					 EEL_DEFAULT_POPUP_MENU_DISPLACEMENT, 
					 EEL_DEFAULT_POPUP_MENU_DISPLACEMENT, 
					 event);
		return TRUE;
 	}
	
	width = widget->allocation.width;
	center = width >> 1;
	if ((event->x < (width / 3)) && nautilus_zoom_control_can_zoom_out (zoom_control)) {
		g_signal_emit (widget, signals[ZOOM_OUT], 0);			
	} else if ((event->x > ((2 * width) / 3)) && nautilus_zoom_control_can_zoom_in (zoom_control)) {
		g_signal_emit (widget, signals[ZOOM_IN], 0);			
	} else if ((event->x >= (center - (width >> 3))) && (event->x <= (center + (width >> 3)))) {
		g_signal_emit (widget, signals[ZOOM_TO_FIT], 0);			
	}

	/* We don't change our state (to reflect the new zoom) here. The zoomable will
	 * call back with the new level. Actually, the callback goes to the view-frame
	 * containing the zoomable which, in turn, emits zoom_level_changed, which
	 * someone (e.g. nautilus_window) picks up and handles by calling into us -
	 * nautilus_zoom_control_set_zoom_level.
	 */	  
  
	return TRUE;
}

/* handle setting the prelight mode */

/* handle enter, leave and motion events to maintain the prelight state */

static gboolean
nautilus_zoom_control_leave_notify (GtkWidget *widget,
			 	    GdkEventCrossing *event)
{
	nautilus_zoom_control_set_prelight_mode (NAUTILUS_ZOOM_CONTROL (widget), PRELIGHT_NONE);
  	return FALSE;
}

static gboolean
nautilus_zoom_control_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	int x, y, x_offset;
	int width, center;
	NautilusZoomControl *zoom_control;
	PrelightMode mode;
	
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	
	gtk_widget_get_pointer (widget, &x, &y);
	
	x_offset = x ;
	width = widget->allocation.width;
	center = width >> 1;

	mode = PRELIGHT_NONE;
	if ((x_offset < (width / 3)) && nautilus_zoom_control_can_zoom_out (zoom_control)) {
		mode = PRELIGHT_MINUS;		
	} else if ((x_offset > ((2 * width) / 3)) && nautilus_zoom_control_can_zoom_in (zoom_control)) {
		mode = PRELIGHT_PLUS;
	} else if ((x_offset >= (center - (width >> 3))) && (x_offset <= (center + (width >> 3)))) {
		mode = PRELIGHT_CENTER;
	}

	nautilus_zoom_control_set_prelight_mode (zoom_control, mode);
	
	return FALSE;
}


/* handle setting the size */
static void
nautilus_zoom_control_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusZoomControl *zoom_control;

	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
	
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
nautilus_zoom_control_set_parameters (NautilusZoomControl *zoom_control,
				      double min_zoom_level,
				      double max_zoom_level,
				      gboolean has_min_zoom_level,
				      gboolean has_max_zoom_level,
				      GList *zoom_levels)
{
	zoom_control->details->min_zoom_level = min_zoom_level;
	zoom_control->details->max_zoom_level = max_zoom_level;
	zoom_control->details->has_min_zoom_level = has_min_zoom_level;
	zoom_control->details->has_max_zoom_level = has_max_zoom_level;

	eel_g_list_free_deep (zoom_control->details->preferred_zoom_levels);
	zoom_control->details->preferred_zoom_levels = zoom_levels;

	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
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

gboolean
nautilus_zoom_control_has_min_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->details->has_min_zoom_level;
}
	
gboolean
nautilus_zoom_control_has_max_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->details->has_max_zoom_level;
}
	
gboolean
nautilus_zoom_control_can_zoom_in (NautilusZoomControl *zoom_control)
{
	return !zoom_control->details->has_max_zoom_level ||
		(zoom_control->details->zoom_level
		 < zoom_control->details->max_zoom_level);
}

gboolean
nautilus_zoom_control_can_zoom_out (NautilusZoomControl *zoom_control)
{
	return !zoom_control->details->has_min_zoom_level ||
		(zoom_control->details->zoom_level
		 > zoom_control->details->min_zoom_level);
}

static void
nautilus_zoom_control_class_init (NautilusZoomControlClass *class)
{
	GtkWidgetClass *widget_class;

	G_OBJECT_CLASS (class)->finalize = nautilus_zoom_control_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	
	widget_class->button_press_event = nautilus_zoom_control_button_press_event;
	widget_class->expose_event = nautilus_zoom_control_expose;
	widget_class->motion_notify_event = nautilus_zoom_control_motion_notify;
	widget_class->size_allocate = nautilus_zoom_control_size_allocate;
  	widget_class->leave_notify_event = nautilus_zoom_control_leave_notify;
	
	signals[ZOOM_IN] =
		g_signal_new ("zoom_in",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusZoomControlClass,
					       zoom_in),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[ZOOM_OUT] =
		g_signal_new ("zoom_out",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusZoomControlClass,
					       zoom_out),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[ZOOM_TO_LEVEL] =
		g_signal_new ("zoom_to_level",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusZoomControlClass,
					       zoom_to_level),
		              NULL, NULL,
		              nautilus_marshal_VOID__DOUBLE,
		              G_TYPE_NONE,
			      1,
			      G_TYPE_DOUBLE);

	signals[ZOOM_TO_FIT] =
		g_signal_new ("zoom_to_fit",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusZoomControlClass,
					       zoom_to_fit),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}
