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

struct NautilusThrobberDetails {
	GList	*image_list;

	GdkPixbuf *quiescent_pixbuf;
	
	int	max_frame;
	int	current_frame;	
	int	timer_task;
};

static void     nautilus_throbber_initialize_class	(NautilusThrobberClass *klass);
static void     nautilus_throbber_initialize		(NautilusThrobber *throbber);
static void	nautilus_throbber_destroy		(GtkObject *object);
static void     nautilus_throbber_draw			(GtkWidget *widget, 
							 GdkRectangle *box);
static int      nautilus_throbber_expose 		(GtkWidget *widget, 
							 GdkEventExpose *event);
static gboolean nautilus_throbber_button_press_event	(GtkWidget *widget, 
							 GdkEventButton *event);
static void	nautilus_throbber_load_images		(NautilusThrobber *throbber);
static void	nautilus_throbber_unload_images		(NautilusThrobber *throbber);
static void	nautilus_throbber_theme_changed 	(gpointer user_data);
static void	nautilus_throbber_size_allocate		(GtkWidget *widget, GtkAllocation *allocation);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusThrobber, nautilus_throbber, GTK_TYPE_EVENT_BOX)

static void
nautilus_throbber_initialize_class (NautilusThrobberClass *throbber_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (throbber_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (throbber_class);
	
	object_class->destroy = nautilus_throbber_destroy;

	widget_class->draw = nautilus_throbber_draw;
	widget_class->expose_event = nautilus_throbber_expose;
	widget_class->button_press_event = nautilus_throbber_button_press_event;
	widget_class->size_allocate = nautilus_throbber_size_allocate;
}

static void 
nautilus_throbber_destroy (GtkObject *object)
{
	nautilus_throbber_unload_images (NAUTILUS_THROBBER (object));

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_throbber_theme_changed,
					      object);

	g_free (NAUTILUS_THROBBER (object)->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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

/* initialize the throbber */
static void
nautilus_throbber_initialize (NautilusThrobber *throbber)
{
	GtkWidget *widget = GTK_WIDGET (throbber);
	int	  throbber_width, throbber_height;

	GTK_WIDGET_UNSET_FLAGS (throbber, GTK_NO_WINDOW);

	gtk_widget_set_events (widget, 
			       gtk_widget_get_events (widget) | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	
	throbber->details = g_new0 (NautilusThrobberDetails, 1);
	
	/* set up the instance variables to appropriate defaults */
	throbber->details->timer_task = -1;
	
	/* allocate the pixmap that holds the image */
	nautilus_throbber_load_images (throbber);
	
	get_throbber_dimensions (throbber, &throbber_width, &throbber_height);
	gtk_widget_set_usize (GTK_WIDGET (throbber), throbber_width, throbber_height);

	/* add a callback for when the theme changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
					  nautilus_throbber_theme_changed,
					  throbber);
}

/* allocate a new throbber */
GtkWidget*
nautilus_throbber_new ()
{
	NautilusThrobber *throbber = gtk_type_new (nautilus_throbber_get_type ());
	return GTK_WIDGET (throbber);
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

	if (throbber->details->timer_task == -1)
		return throbber->details->quiescent_pixbuf;
	
	if (throbber->details->image_list == NULL)
		return NULL;

	element = g_list_nth (throbber->details->image_list, throbber->details->current_frame);
	
	return (GdkPixbuf*) element->data;
}

/* draw the throbber into the passed-in rectangle */

static void
draw_throbber_image (GtkWidget *widget, GdkRectangle *box)
{
	NautilusThrobber *throbber;
	GdkPixbuf *pixbuf;
		
	throbber = NAUTILUS_THROBBER (widget);

	pixbuf = select_throbber_image (throbber);
	
	draw_pixbuf (pixbuf, widget->window, box->x, box->y);
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

/* here's the actual timeout task to bump the frame and schedule a redraw */

static int 
bump_throbber_frame (NautilusThrobber *throbber)
{
	throbber->details->current_frame += 1;
	if (throbber->details->current_frame > throbber->details->max_frame) {
		throbber->details->current_frame = 0;
	}	
	
	gtk_widget_queue_draw (GTK_WIDGET (throbber));
	return TRUE;
}


/* routines to start and stop the throbber */

void
nautilus_throbber_start (NautilusThrobber *throbber)
{
	if (throbber->details->timer_task != -1)
		gtk_timeout_remove(throbber->details->timer_task);

	throbber->details->timer_task = gtk_timeout_add (100, (GtkFunction) bump_throbber_frame, throbber);
}

void
nautilus_throbber_stop (NautilusThrobber *throbber)
{
	if (throbber->details->timer_task != -1)
		gtk_timeout_remove(throbber->details->timer_task);
	
	throbber->details->timer_task = -1;
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
	char *throbber_frame_name;
	GdkPixbuf *pixbuf;
	
	nautilus_throbber_unload_images (throbber);

	throbber->details->quiescent_pixbuf = load_themed_image ("throbber/rest.png");
	/* images are of the form throbber/001.png, 002.png, etc, so load them into a list
	 * until we get an error.
	 */
	throbber->details->max_frame = -1;
	index = 1;
	while (TRUE) {
		throbber_frame_name = make_throbber_frame_name (index);
		pixbuf = load_themed_image (throbber_frame_name);
		g_free (throbber_frame_name);
		if (pixbuf == NULL)
			break;
	
		throbber->details->image_list = g_list_append (throbber->details->image_list, pixbuf);
		index += 1;
		throbber->details->max_frame += 1;	
	}
	
}

/* handle button presses */

static gboolean
nautilus_throbber_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	/*	
	NautilusThrobber *throbber = NAUTILUS_THROBBER (widget);  
	*/
	return TRUE;
}

/* handle setting the size */
static void
nautilus_throbber_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	int throbber_width, throbber_height;
	NautilusThrobber *throbber = NAUTILUS_THROBBER (widget);

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	get_throbber_dimensions (throbber, &throbber_width, &throbber_height);
	
	widget->allocation.width = throbber_width;
   	widget->allocation.height = throbber_height;
}

