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
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-theme.h>

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

	GdkFont *label_font;

	gboolean marking_menu_items;
};


static guint signals[LAST_SIGNAL];

static void     nautilus_zoom_control_initialize_class 	 (NautilusZoomControlClass *klass);
static void     nautilus_zoom_control_initialize       	 (NautilusZoomControl *zoom_control);
static void	nautilus_zoom_control_destroy		 (GtkObject *object);
static void     nautilus_zoom_control_draw 	       	 (GtkWidget *widget, 
							  GdkRectangle *box);
static int	nautilus_zoom_control_expose 		 (GtkWidget *widget, 
							  GdkEventExpose *event);
static 		gboolean nautilus_zoom_control_button_press_event (GtkWidget *widget, 
							  GdkEventButton *event);
static int	nautilus_zoom_control_leave_notify	 (GtkWidget        *widget,
			 				  GdkEventCrossing *event);
static gboolean nautilus_zoom_control_motion_notify       (GtkWidget        *widget,
						     	  GdkEventMotion   *event);

static void	nautilus_zoom_control_load_images	 (NautilusZoomControl *zoom_control);
static void	nautilus_zoom_control_unload_images	 (NautilusZoomControl *zoom_control);
static void	nautilus_zoom_control_theme_changed 	 (gpointer user_data);
static void	nautilus_zoom_control_size_allocate	 (GtkWidget *widget, GtkAllocation *allocation);
static void	nautilus_zoom_control_set_prelight_mode	 (NautilusZoomControl *zoom_control, PrelightMode mode);


void            draw_number		                 (GtkWidget *widget, 
							  GdkRectangle *box);

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

EEL_DEFINE_CLASS_BOILERPLATE (NautilusZoomControl, nautilus_zoom_control, GTK_TYPE_EVENT_BOX)

static void
nautilus_zoom_control_initialize_class (NautilusZoomControlClass *zoom_control_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (zoom_control_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (zoom_control_class);
	
	object_class->destroy = nautilus_zoom_control_destroy;

	widget_class->draw = nautilus_zoom_control_draw;
	widget_class->expose_event = nautilus_zoom_control_expose;
	widget_class->button_press_event = nautilus_zoom_control_button_press_event;
  	widget_class->leave_notify_event = nautilus_zoom_control_leave_notify;
	widget_class->motion_notify_event = nautilus_zoom_control_motion_notify;

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
				eel_gtk_marshal_NONE__DOUBLE,
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
	eel_g_list_free_deep (NAUTILUS_ZOOM_CONTROL (object)->details->preferred_zoom_levels);
	NAUTILUS_ZOOM_CONTROL (object)->details->preferred_zoom_levels = NULL;

	/* deallocate pixbufs */
	nautilus_zoom_control_unload_images (NAUTILUS_ZOOM_CONTROL (object));

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_zoom_control_theme_changed,
					      object);

	g_free (NAUTILUS_ZOOM_CONTROL (object)->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
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
	zoom_control->details->has_min_zoom_level = TRUE;
	zoom_control->details->has_max_zoom_level = TRUE;
	zoom_control->details->preferred_zoom_levels = NULL;

	/* allocate the pixmap that holds the image */
	nautilus_zoom_control_load_images (zoom_control);
	
	zoom_width = get_zoom_width (zoom_control);
	gtk_widget_set_usize (GTK_WIDGET (zoom_control), zoom_width, -1);

	/* add a callback for when the theme changes */
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
					  nautilus_zoom_control_theme_changed,
					  zoom_control);	
	
	/* enable mouse motion events */
	gtk_widget_add_events (GTK_WIDGET (zoom_control), GDK_POINTER_MOTION_MASK);
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
	GdkGC* temp_gc; 
	int x, y, percent; 
	int char_height, char_width, char_offset;
	int num_v_offset, num_h_offset;
	NautilusZoomControl *zoom_control;
	GdkPixbuf *number_pixbuf;

	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	number_pixbuf = NULL;
	
	num_v_offset = get_zoom_offset ("number_v_offset");
	num_h_offset = get_zoom_offset ("number_h_offset");
	char_width = get_zoom_offset ("digit_width");
	if (char_width == 0)
		char_width = 6;
		
	percent = floor((100.0 * zoom_control->details->zoom_level) + .5);
	g_snprintf(buffer, 8, "%d", percent);

	y = 1 + num_v_offset + (widget->allocation.height >> 1);
	x = num_h_offset + ((widget->allocation.width - char_width * strlen(buffer)) >> 1);  
	
	temp_gc = gdk_gc_new(widget->window);
	
	if (zoom_control->details->number_strip) {
		cur_char = &buffer[0];
		char_height = gdk_pixbuf_get_height (zoom_control->details->number_strip);
		
		number_pixbuf = zoom_control->details->number_strip;
		if (zoom_control->details->prelight_mode == PRELIGHT_CENTER) {
			number_pixbuf = eel_create_spotlight_pixbuf (number_pixbuf);
		}
		
		while (*cur_char) {
			/* draw the character */
			
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
	} else {				
		x = num_h_offset + ((widget->allocation.width - gdk_string_width(zoom_control->details->label_font, buffer)) >> 1);  
		gdk_draw_string (widget->window, zoom_control->details->label_font, temp_gc, x, y, &buffer[0]);
	}
	
	if (number_pixbuf != zoom_control->details->number_strip) {
		gdk_pixbuf_unref (number_pixbuf);
	}
	
	gdk_gc_unref(temp_gc);
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
		gdk_pixbuf_unref (temp_pixbuf);
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
		gdk_pixbuf_unref (zoom_control->details->zoom_body_image);
	}
	
	if (zoom_control->details->zoom_decrement_image) {
		gdk_pixbuf_unref (zoom_control->details->zoom_decrement_image);
	}
	
	if (zoom_control->details->zoom_increment_image) {
		gdk_pixbuf_unref (zoom_control->details->zoom_increment_image);
	}

	if (zoom_control->details->number_strip != NULL) {
		gdk_pixbuf_unref (zoom_control->details->number_strip);
	}

	if (zoom_control->details->label_font != NULL) {
		gdk_font_unref(zoom_control->details->label_font);
		zoom_control->details->label_font = NULL;
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
	
	if (zoom_control->details->number_strip == NULL) {
		/* Note to localizers: this font is used for the number in the
		 * zoom control widget.
		 */
		zoom_control->details->label_font = gdk_fontset_load (_("-bitstream-courier-medium-r-normal-*-9-*-*-*-*-*-*-*"));
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

	zoom_level = * (double *) gtk_object_get_data (GTK_OBJECT (item), "zoom_level");

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
		gtk_signal_emit (GTK_OBJECT (zoom_control), signals[ZOOM_TO_LEVEL], zoom_level);
	}
}

static GtkRadioMenuItem *
create_zoom_menu_item (GtkMenu *menu, GtkWidget *widget, float zoom_level,
		       GtkRadioMenuItem *previous_radio_item)
{
	GtkWidget *menu_item;
	gchar     *item_text;
	double    *zoom_level_ptr;
	NautilusZoomControl *zoom_control;
	GSList	  *radio_item_group;
	int	  percent;
	
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	/* Set flag so that callback isn't activated when set_active called
	 * to set toggle state of other radio items.
	 */
	zoom_control->details->marking_menu_items = TRUE;

	/* This is marked for localization in case the % sign is not
	 * appropriate in some locale. I guess that's unlikely.
	 */
	percent = floor((100.0 * zoom_level) + .5);
	item_text = g_strdup_printf (_("%d%%"), percent);

	radio_item_group = previous_radio_item == NULL
		? NULL
		: gtk_radio_menu_item_group (previous_radio_item);
	menu_item = gtk_radio_menu_item_new_with_label (radio_item_group, item_text);

	zoom_level_ptr = g_new (double, 1);
	*zoom_level_ptr = zoom_level;

	gtk_check_menu_item_set_show_toggle (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), 
					zoom_level == zoom_control->details->zoom_level);
	
	gtk_object_set_data_full (GTK_OBJECT (menu_item), "zoom_level", zoom_level_ptr, g_free);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (zoom_menu_callback),
			    zoom_control);

  	gtk_widget_show (menu_item);
	gtk_menu_append (menu, menu_item);

	zoom_control->details->marking_menu_items = FALSE;

	return GTK_RADIO_MENU_ITEM (menu_item);
}

static GtkMenu*
create_zoom_menu(GtkWidget *zoom_control)
{
	GList *p;
	GtkMenu *menu;
	GtkRadioMenuItem *menu_item;

	menu = GTK_MENU (gtk_menu_new ());

	p = NAUTILUS_ZOOM_CONTROL (zoom_control)->details->preferred_zoom_levels;

	menu_item = NULL;
	while (p != NULL) {
		menu_item = create_zoom_menu_item (menu, zoom_control, *(float *) p->data, menu_item);
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
		eel_pop_up_context_menu (create_zoom_menu (widget), 
					      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT, 
					      EEL_DEFAULT_POPUP_MENU_DISPLACEMENT, 
					      event);
		return TRUE;	  
 	}
	
	if ((event->x < (width / 3)) && nautilus_zoom_control_can_zoom_out (zoom_control)) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_OUT]);			
	} else if ((event->x > ((2 * width) / 3)) && nautilus_zoom_control_can_zoom_in (zoom_control)) {
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

/* handle setting the prelight mode */

/* handle enter, leave and motion events to maintain the prelight state */

static gint
nautilus_zoom_control_leave_notify (GtkWidget        *widget,
			 	    GdkEventCrossing *event)
{
	nautilus_zoom_control_set_prelight_mode (NAUTILUS_ZOOM_CONTROL (widget), PRELIGHT_NONE);
  	return FALSE;
}

static gboolean nautilus_zoom_control_motion_notify (GtkWidget *widget, GdkEventMotion   *event)
{
	int x, y, x_offset;
	int width, center;
	NautilusZoomControl *zoom_control;
	PrelightMode mode;
	
	zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	
	gtk_widget_get_pointer(widget, &x, &y);
	
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
	
	return TRUE;
}


/* handle setting the size */
static void
nautilus_zoom_control_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL (widget);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
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
