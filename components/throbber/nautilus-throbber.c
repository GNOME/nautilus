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
 * This is the throbber (for busy feedback) for the location bar
 *
 */

#include <config.h>
#include "nautilus-throbber.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <math.h>

#define THROBBER_DEFAULT_TIMEOUT 100	/* Milliseconds Per Frame */

struct NautilusThrobberDetails {
	BonoboObject *control;
	BonoboPropertyBag *property_bag;
	GList	*image_list;

	GdkPixbuf *quiescent_pixbuf;
	
	int	max_frame;
	int	delay;
	int	current_frame;	
	guint	timer_task;
	
	gboolean ready;
	gboolean small_mode;

	gboolean button_in;
	gboolean button_down;
};


static void     nautilus_throbber_initialize_class	 (NautilusThrobberClass *klass);
static void     nautilus_throbber_initialize		 (NautilusThrobber *throbber);
static void	nautilus_throbber_destroy		 (GtkObject *object);
static void     nautilus_throbber_draw			 (GtkWidget *widget, 
							  GdkRectangle *box);
static int      nautilus_throbber_expose 		 (GtkWidget *widget, 
							  GdkEventExpose *event);
static gboolean nautilus_throbber_button_press_event	 (GtkWidget *widget, 
							  GdkEventButton *event);
static gboolean nautilus_throbber_button_release_event	 (GtkWidget *widget, 
							  GdkEventButton *event);
static gboolean nautilus_throbber_enter_notify_event	 (GtkWidget *widget, 
							  GdkEventCrossing *event);
static gboolean nautilus_throbber_leave_notify_event	 (GtkWidget *widget, 
							  GdkEventCrossing *event);

static void nautilus_throbber_map				 (GtkWidget *widget); 

static void	nautilus_throbber_load_images		 (NautilusThrobber *throbber);
static void	nautilus_throbber_unload_images		 (NautilusThrobber *throbber);
static void	nautilus_throbber_theme_changed 	 (gpointer user_data);
static void	nautilus_throbber_size_allocate		 (GtkWidget *widget, GtkAllocation *allocation);
static void	nautilus_throbber_size_request		 (GtkWidget *widget, GtkRequisition *requisition);
static void     nautilus_throbber_remove_update_callback (NautilusThrobber *throbber);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusThrobber, nautilus_throbber, GTK_TYPE_EVENT_BOX)

static void
nautilus_throbber_initialize_class (NautilusThrobberClass *throbber_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (throbber_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (throbber_class);
	
	object_class->destroy = nautilus_throbber_destroy;

	widget_class->draw = nautilus_throbber_draw;
	widget_class->expose_event = nautilus_throbber_expose;
	widget_class->button_press_event = nautilus_throbber_button_press_event;
	widget_class->button_release_event = nautilus_throbber_button_release_event;
	widget_class->enter_notify_event = nautilus_throbber_enter_notify_event;
	widget_class->leave_notify_event = nautilus_throbber_leave_notify_event;
	widget_class->size_allocate = nautilus_throbber_size_allocate;
	widget_class->size_request = nautilus_throbber_size_request;	
	widget_class->map = nautilus_throbber_map;
}

/* routines to handle setting and getting the configuration properties of the Bonobo control */

enum {
	THROBBING,
	LOCATION
} MyArgs;


static gboolean
is_throbbing (NautilusThrobber *throbber)
{
	return throbber->details->timer_task != 0;
}

static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer user_data)
{
	NautilusThrobber *throbber = NAUTILUS_THROBBER (user_data);

	switch (arg_id) {
		case THROBBING:
		{
			BONOBO_ARG_SET_BOOLEAN (arg, throbber->details->timer_task != 0);
			break;
		}

		case LOCATION:
		{
			char *location = nautilus_theme_get_theme_data ("throbber", "url");
			if (location != NULL) {
				BONOBO_ARG_SET_STRING (arg, location);
				g_free (location);
			} else {
				BONOBO_ARG_SET_STRING (arg, "");			
			}
		
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
	NautilusThrobber *throbber = NAUTILUS_THROBBER (user_data);
	switch (arg_id) {
		case THROBBING:
		{
			gboolean throbbing;

			throbbing = BONOBO_ARG_GET_BOOLEAN (arg);
			
			if (throbbing != is_throbbing (throbber)) {
				if (throbbing) {
					nautilus_throbber_start (throbber);
				} else {
					nautilus_throbber_stop (throbber);
				}
			}
						
			break;
		}

		default:
			g_warning ("Unhandled arg %d", arg_id);
			break;
	}
}

/* handle destroying the throbber */
static void 
nautilus_throbber_destroy (GtkObject *object)
{
	NautilusThrobber *throbber = NAUTILUS_THROBBER (object);

	nautilus_bonobo_object_force_destroy_later (throbber->details->control);
	
	nautilus_throbber_remove_update_callback (throbber);
	nautilus_throbber_unload_images (throbber);

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					 nautilus_throbber_theme_changed,
					 object);
	
	if (throbber->details->property_bag) {
		bonobo_object_unref (BONOBO_OBJECT (throbber->details->property_bag));
	}
	
	g_free (throbber->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

BonoboObject*
nautilus_throbber_get_control (NautilusThrobber *throbber)
{
	return throbber->details->control;
}

/* loop through all the images taking their union to compute the width and height of the throbber */
static void
get_throbber_dimensions (NautilusThrobber *throbber, int *throbber_width, int* throbber_height)
{
	int current_width, current_height;
	int pixbuf_width, pixbuf_height;
	GList *current_entry;
	GdkPixbuf *pixbuf;
	
	/* start with the quiescent image */
	current_width = gdk_pixbuf_get_width (throbber->details->quiescent_pixbuf);
	current_height = gdk_pixbuf_get_height (throbber->details->quiescent_pixbuf);

	/* loop through all the installed images, taking the union */
	current_entry = throbber->details->image_list;
	while (current_entry != NULL) {	
		pixbuf = (GdkPixbuf*) current_entry->data;
		pixbuf_width = gdk_pixbuf_get_width (pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (pixbuf);
		
		if (pixbuf_width > current_width) {
			current_width = pixbuf_width;
		}
		
		if (pixbuf_height > current_height) {
			current_height = pixbuf_height;
		}
		
		current_entry = current_entry->next;
	}
		
	/* return the result */
	*throbber_width = current_width;
	*throbber_height = current_height;
}

static void
null_pointer_callback (GtkObject *object,
		       gpointer callback_data)
{
	* (gpointer *) callback_data = NULL;
}

/* initialize the throbber */
static void
nautilus_throbber_initialize (NautilusThrobber *throbber)
{
	char *delay_str;
	GtkWidget *widget = GTK_WIDGET (throbber);
	
	
	GTK_WIDGET_UNSET_FLAGS (throbber, GTK_NO_WINDOW);

	gtk_widget_set_events (widget, 
			       gtk_widget_get_events (widget)
			       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	
	throbber->details = g_new0 (NautilusThrobberDetails, 1);
	
	/* set up the delay from the theme */
	delay_str = nautilus_theme_get_theme_data ("throbber", "delay");
	
	if (delay_str) {
		throbber->details->delay = atoi (delay_str);
		g_free (delay_str);
	} else {
		throbber->details->delay = THROBBER_DEFAULT_TIMEOUT;		
	}
	
	/* make the bonobo control */
	throbber->details->control = BONOBO_OBJECT (bonobo_control_new (widget));
	gtk_signal_connect_while_alive (GTK_OBJECT (throbber->details->control),
					"destroy",
					null_pointer_callback,
					&throbber->details->control,
					GTK_OBJECT (throbber));
	
	/* attach a property bag with the configure property */
	throbber->details->property_bag = bonobo_property_bag_new (get_bonobo_properties, 
								   set_bonobo_properties, throbber);
	bonobo_control_set_properties (BONOBO_CONTROL (throbber->details->control), 
				       throbber->details->property_bag);
	
	bonobo_property_bag_add (throbber->details->property_bag, "throbbing", THROBBING, BONOBO_ARG_BOOLEAN, NULL,
				 "Throbber active", 0);
	bonobo_property_bag_add (throbber->details->property_bag, "location", LOCATION, BONOBO_ARG_STRING, NULL,
				 "associated URL", 0);
	
	/* allocate the pixmap that holds the image */
	nautilus_throbber_load_images (throbber);
	gtk_widget_show (widget);
	
	/* add a callback for when the theme changes */
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
				      nautilus_throbber_theme_changed,
				      throbber);
}

/* allocate a new throbber */
GtkWidget *
nautilus_throbber_new (void)
{
	return gtk_widget_new (nautilus_throbber_get_type (), NULL);
}

/* handler for handling theme changes */
static void
nautilus_throbber_theme_changed (gpointer user_data)
{
	NautilusThrobber *throbber;

	throbber = NAUTILUS_THROBBER (user_data);
	gtk_widget_hide (GTK_WIDGET (throbber));
	nautilus_throbber_load_images (throbber);
	gtk_widget_show (GTK_WIDGET (throbber));	
	gtk_widget_queue_resize ( GTK_WIDGET (throbber));
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

/* here's the routine that selects the image to draw, based on the throbber's state */

static GdkPixbuf *
select_throbber_image (NautilusThrobber *throbber)
{
	GList *element;

	if (throbber->details->timer_task == 0) {
		return gdk_pixbuf_ref (throbber->details->quiescent_pixbuf);
	}
	
	if (throbber->details->image_list == NULL) {
		return NULL;
	}

	element = g_list_nth (throbber->details->image_list, throbber->details->current_frame);
	
	return gdk_pixbuf_ref (element->data);
}

/* draw the throbber into the passed-in rectangle */

static void
draw_throbber_image (GtkWidget *widget, GdkRectangle *box)
{
	NautilusThrobber *throbber;
	GdkPixbuf *pixbuf, *massaged_pixbuf;
	int window_width, window_height;
	int x_offset, y_offset;
		
	throbber = NAUTILUS_THROBBER (widget);
	if (!throbber->details->ready) {
		return;
	}
		
	/* clear the entire gdk window to avoid messing up gradient themes due to bonobo bug  */
	/* only do this once per cycle to minimize flashing */
	gdk_window_get_size (widget->window, &window_width, &window_height);
	
	if (throbber->details->current_frame == 0) {
		gdk_window_clear_area (widget->window,
			       0,
			       0,
			       window_width,
			       window_height);
	}
	
	pixbuf = select_throbber_image (throbber);
	if (pixbuf == NULL) {
		return;
	}

	if (throbber->details->button_in) {
		if (throbber->details->button_down) {
			massaged_pixbuf = eel_create_darkened_pixbuf (pixbuf, 0.8 * 255, 0.8 * 255);
		} else {
			massaged_pixbuf = eel_create_spotlight_pixbuf (pixbuf);
		}
		gdk_pixbuf_unref (pixbuf);
		pixbuf = massaged_pixbuf;
	}
	
	/* center the throbber image in the gdk window */	
	x_offset = (window_width - gdk_pixbuf_get_width (pixbuf)) / 2;
	y_offset = (window_height - gdk_pixbuf_get_height (pixbuf)) / 2;
	
	draw_pixbuf (pixbuf, widget->window, box->x + x_offset, box->y + y_offset);
	
	gdk_pixbuf_unref (pixbuf);
}

static void
nautilus_throbber_draw (GtkWidget *widget, GdkRectangle *box)
{ 
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_THROBBER (widget));
	
	draw_throbber_image (widget, box);	
}

/* handle expose events */

static int
nautilus_throbber_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GdkRectangle box;
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_THROBBER (widget), FALSE);

	box.x = 0; box.y = 0;
	box.width = widget->allocation.width;
	box.height = widget->allocation.height;

	draw_throbber_image (widget, &box);	
	
	return FALSE;
}

static void
nautilus_throbber_map (GtkWidget *widget)
{
	NautilusThrobber *throbber;
	
	throbber = NAUTILUS_THROBBER (widget);
	
	EEL_CALL_PARENT (GTK_WIDGET_CLASS, map, (widget));
	throbber->details->ready = TRUE;
}

/* here's the actual timeout task to bump the frame and schedule a redraw */

static gboolean 
bump_throbber_frame (gpointer callback_data)
{
	NautilusThrobber *throbber;

	throbber = NAUTILUS_THROBBER (callback_data);
	if (!throbber->details->ready) {
		return TRUE;
	}

	throbber->details->current_frame += 1;
	if (throbber->details->current_frame > throbber->details->max_frame - 1) {
		throbber->details->current_frame = 0;
	}

	gtk_widget_queue_draw (GTK_WIDGET (throbber));
	return TRUE;
}


/* routines to start and stop the throbber */

void
nautilus_throbber_start (NautilusThrobber *throbber)
{
	if (is_throbbing (throbber)) {
		return;
	}

	if (throbber->details->timer_task != 0) {
		gtk_timeout_remove (throbber->details->timer_task);
	}
	
	/* reset the frame count */
	throbber->details->current_frame = 0;
	throbber->details->timer_task = gtk_timeout_add (throbber->details->delay,
							 bump_throbber_frame,
							 throbber);
}

static void
nautilus_throbber_remove_update_callback (NautilusThrobber *throbber)
{
	if (throbber->details->timer_task != 0) {
		gtk_timeout_remove (throbber->details->timer_task);
	}
	
	throbber->details->timer_task = 0;
}

void
nautilus_throbber_stop (NautilusThrobber *throbber)
{
	if (!is_throbbing (throbber)) {
		return;
	}

	nautilus_throbber_remove_update_callback (throbber);
	gtk_widget_queue_draw (GTK_WIDGET (throbber));

}

/* routines to load the images used to draw the throbber */

/* unload all the images, and the list itself */

static void
nautilus_throbber_unload_images (NautilusThrobber *throbber)
{
	GList *current_entry;

	if (throbber->details->quiescent_pixbuf != NULL) {
		gdk_pixbuf_unref (throbber->details->quiescent_pixbuf);
		throbber->details->quiescent_pixbuf = NULL;
	}

	/* unref all the images in the list, and then let go of the list itself */
	current_entry = throbber->details->image_list;
	while (current_entry != NULL) {
		gdk_pixbuf_unref ((GdkPixbuf*) current_entry->data);
		current_entry = current_entry->next;
	}
	
	g_list_free (throbber->details->image_list);
	throbber->details->image_list = NULL;
}

static GdkPixbuf*
load_themed_image (const char *file_name, const char *image_theme, gboolean small_mode)
{
	GdkPixbuf *pixbuf, *temp_pixbuf;
	char *image_path;
	
	if (image_theme == NULL) {
		image_path = nautilus_theme_get_image_path (file_name);
	} else {
		image_path = nautilus_theme_get_image_path_from_theme (file_name, image_theme);	
	}
	
	if (image_path) {
		pixbuf = gdk_pixbuf_new_from_file (image_path);
		
		if (small_mode && pixbuf) {
			temp_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
							       gdk_pixbuf_get_width (pixbuf) / 2,
							       gdk_pixbuf_get_height (pixbuf) / 2,
							       GDK_INTERP_BILINEAR);
			gdk_pixbuf_unref (pixbuf);
			pixbuf = temp_pixbuf;
		}
		
		g_free (image_path);
		return pixbuf;
	}
	return NULL;
}

/* utility to make the throbber frame name from the index */

static char *
make_throbber_frame_name (int index)
{
	return g_strdup_printf ("throbber/%03d.png", index);
}

/* load all of the images of the throbber sequentially */
static void
nautilus_throbber_load_images (NautilusThrobber *throbber)
{
	int index;
	char *throbber_frame_name, *image_theme, *frames;
	GdkPixbuf *pixbuf;
	GList *image_list;
	
	nautilus_throbber_unload_images (throbber);

	image_theme = nautilus_theme_get_theme_data ("throbber", "image_theme");
	throbber->details->quiescent_pixbuf = load_themed_image ("throbber/rest.png", image_theme, throbber->details->small_mode);

	/* images are of the form throbber/001.png, 002.png, etc, so load them into a list */

	frames = nautilus_theme_get_theme_data ("throbber", "frame_count");
	if (frames != NULL) {
		throbber->details->max_frame = atoi (frames);
		g_free (frames);
	} else {
		throbber->details->max_frame = 16;
	}

	image_list = NULL;
	for (index = 1; index <= throbber->details->max_frame; index++) {
		throbber_frame_name = make_throbber_frame_name (index);
		pixbuf = load_themed_image (throbber_frame_name, image_theme, throbber->details->small_mode);
		g_free (throbber_frame_name);
		if (pixbuf == NULL) {
			throbber->details->max_frame = index - 1;
			break;
		}
		image_list = g_list_prepend (image_list, pixbuf);
	}
	throbber->details->image_list = g_list_reverse (image_list);

	g_free (image_theme);
}

static gboolean
nautilus_throbber_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
	NautilusThrobber *throbber;

	throbber = NAUTILUS_THROBBER (widget);

	if (!throbber->details->button_in) {
		throbber->details->button_in = TRUE;
		gtk_widget_queue_draw (widget);
	}

	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, enter_notify_event, (widget, event));
}

static gboolean
nautilus_throbber_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event)
{
	NautilusThrobber *throbber;

	throbber = NAUTILUS_THROBBER (widget);

	if (throbber->details->button_in) {
		throbber->details->button_in = FALSE;
		gtk_widget_queue_draw (widget);
	}

	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, leave_notify_event, (widget, event));
}

/* handle button presses by posting a change on the "location" property */

static gboolean
nautilus_throbber_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	NautilusThrobber *throbber;

	throbber = NAUTILUS_THROBBER (widget);

	if (event->button == 1) {
		throbber->details->button_down = TRUE;
		throbber->details->button_in = TRUE;
		gtk_widget_queue_draw (widget);
		return TRUE;
	}

	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_press_event, (widget, event));
}

static gboolean
nautilus_throbber_button_release_event (GtkWidget *widget, GdkEventButton *event)
{	
	NautilusThrobber *throbber;
	char *location;
	BonoboArg *location_arg;
	
	throbber = NAUTILUS_THROBBER (widget);

	if (event->button == 1) {
		if (throbber->details->button_in) {
			location = nautilus_theme_get_theme_data ("throbber", "url");
			if (location != NULL) {
				location_arg = bonobo_arg_new (BONOBO_ARG_STRING);
				BONOBO_ARG_SET_STRING (location_arg, location);			
				bonobo_property_bag_notify_listeners
					(throbber->details->property_bag, "location", location_arg, NULL);
				bonobo_arg_release (location_arg);
				g_free (location);
			}
		}
		throbber->details->button_down = FALSE;
		gtk_widget_queue_draw (widget);
		return TRUE;
	}
	
	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_release_event, (widget, event));
}

void
nautilus_throbber_set_small_mode (NautilusThrobber *throbber, gboolean new_mode)
{
	int throbber_width, throbber_height;
	
	if (new_mode != throbber->details->small_mode) {
		throbber->details->small_mode = new_mode;
		nautilus_throbber_load_images (throbber);

		get_throbber_dimensions (throbber, &throbber_width, &throbber_height);
		gtk_widget_set_usize (GTK_WIDGET (throbber), throbber_width, throbber_height);		
		gtk_widget_queue_resize (GTK_WIDGET (throbber));
	}
}

/* handle setting the size */
static void
nautilus_throbber_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	int throbber_width, throbber_height;
	NautilusThrobber *throbber = NAUTILUS_THROBBER (widget);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	get_throbber_dimensions (throbber, &throbber_width, &throbber_height);
	
	/* allocate some extra margin so we don't butt up against toolbar edges */	
	widget->allocation.width = throbber_width + 8;
   	widget->allocation.height = throbber_height;	
}

/* handle setting the size */
static void
nautilus_throbber_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	int throbber_width, throbber_height;
	NautilusThrobber *throbber = NAUTILUS_THROBBER (widget);

	get_throbber_dimensions (throbber, &throbber_width, &throbber_height);
	
	/* allocate some extra margin so we don't butt up against toolbar edges */
	requisition->width = throbber_width + 8;
   	requisition->height = throbber_height;	
}
