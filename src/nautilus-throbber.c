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
#include <math.h>

#include "nautilus-throbber.h"

#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-accessibility.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <gtk/gtkicontheme.h>

#define THROBBER_DEFAULT_TIMEOUT 100	/* Milliseconds Per Frame */

struct NautilusThrobberDetails {
	GList	*image_list;

	GdkPixbuf *quiescent_pixbuf;
	
	int	max_frame;
	int	delay;
	int	current_frame;	
	guint	timer_task;
	
	gboolean ready;
	gboolean small_mode;
};


static void nautilus_throbber_load_images            (NautilusThrobber *throbber);
static void nautilus_throbber_unload_images          (NautilusThrobber *throbber);
static void nautilus_throbber_theme_changed          (GtkIconTheme   *icon_theme,
						      NautilusThrobber *throbber);
static void nautilus_throbber_remove_update_callback (NautilusThrobber *throbber);
static AtkObject *nautilus_throbber_get_accessible   (GtkWidget *widget);

GNOME_CLASS_BOILERPLATE (NautilusThrobber, nautilus_throbber,
			 GtkEventBox, GTK_TYPE_EVENT_BOX)



static gboolean
is_throbbing (NautilusThrobber *throbber)
{
	return throbber->details->timer_task != 0;
}

/* loop through all the images taking their union to compute the width and height of the throbber */
static void
get_throbber_dimensions (NautilusThrobber *throbber, int *throbber_width, int* throbber_height)
{
	int current_width, current_height;
	int pixbuf_width, pixbuf_height;
	GList *image_list;
	GdkPixbuf *pixbuf;
	
	current_width = 0;
	current_height = 0;

	if (throbber->details->quiescent_pixbuf != NULL) {
		/* start with the quiescent image */
		current_width = gdk_pixbuf_get_width (throbber->details->quiescent_pixbuf);
		current_height = gdk_pixbuf_get_height (throbber->details->quiescent_pixbuf);
	}

	/* union with the animation image */
	image_list = throbber->details->image_list;
	if (image_list != NULL) {
		pixbuf = GDK_PIXBUF (image_list->data);
		pixbuf_width = gdk_pixbuf_get_width (pixbuf);
		pixbuf_height = gdk_pixbuf_get_height (pixbuf);
		
		if (pixbuf_width > current_width) {
			current_width = pixbuf_width;
		}
		
		if (pixbuf_height > current_height) {
			current_height = pixbuf_height;
		}
	}
		
	/* return the result */
	*throbber_width = current_width;
	*throbber_height = current_height;
}

static void
nautilus_throbber_instance_init (NautilusThrobber *throbber)
{
	GtkWidget *widget = GTK_WIDGET (throbber);
	
	
	GTK_WIDGET_UNSET_FLAGS (throbber, GTK_NO_WINDOW);

	gtk_widget_set_events (widget, 
			       gtk_widget_get_events (widget)
			       | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
			       | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	
	throbber->details = g_new0 (NautilusThrobberDetails, 1);
	
	throbber->details->delay = THROBBER_DEFAULT_TIMEOUT;
	
	g_signal_connect (gtk_icon_theme_get_default (),
			  "changed",
			  G_CALLBACK (nautilus_throbber_theme_changed),
			  throbber);


	nautilus_throbber_load_images (throbber);
	gtk_widget_show (widget);
}

/* handler for handling theme changes */
static void
nautilus_throbber_theme_changed (GtkIconTheme *icon_theme, NautilusThrobber *throbber)
{
	gtk_widget_hide (GTK_WIDGET (throbber));
	nautilus_throbber_load_images (throbber);
	gtk_widget_show (GTK_WIDGET (throbber));	
	gtk_widget_queue_resize ( GTK_WIDGET (throbber));
}

/* here's the routine that selects the image to draw, based on the throbber's state */

static GdkPixbuf *
select_throbber_image (NautilusThrobber *throbber)
{
	GList *element;

	if (throbber->details->timer_task == 0) {
		if (throbber->details->quiescent_pixbuf == NULL) {
			return NULL;
		} else {
			return g_object_ref (throbber->details->quiescent_pixbuf);
		}
	}
	
	if (throbber->details->image_list == NULL) {
		return NULL;
	}
	
	element = g_list_nth (throbber->details->image_list, throbber->details->current_frame);
	
	return g_object_ref (element->data);
}

/* handle expose events */

static int
nautilus_throbber_expose (GtkWidget *widget, GdkEventExpose *event)
{
	NautilusThrobber *throbber;
	GdkPixbuf *pixbuf;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;

	g_return_val_if_fail (NAUTILUS_IS_THROBBER (widget), FALSE);

	throbber = NAUTILUS_THROBBER (widget);
	if (!throbber->details->ready) {
		return FALSE;
	}

	pixbuf = select_throbber_image (throbber);
	if (pixbuf == NULL) {
		return FALSE;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Compute the offsets for the image centered on our allocation */
	x_offset = (widget->allocation.width - width) / 2;
	y_offset = (widget->allocation.height - height) / 2;

	pix_area.x = x_offset;
	pix_area.y = y_offset;
	pix_area.width = width;
	pix_area.height = height;

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest)) {
		g_object_unref (pixbuf);
		return FALSE;
	}
	
	gdk_draw_pixbuf (widget->window, NULL, pixbuf,
			 dest.x - x_offset, dest.y - y_offset,
			 dest.x, dest.y,
			 dest.width, dest.height,
			 GDK_RGB_DITHER_MAX, 0, 0);

	g_object_unref (pixbuf);

	return FALSE;
}

static void
nautilus_throbber_map (GtkWidget *widget)
{
	NautilusThrobber *throbber;
	
	throbber = NAUTILUS_THROBBER (widget);
	
	GNOME_CALL_PARENT (GTK_WIDGET_CLASS, map, (widget));
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

	gtk_widget_draw (GTK_WIDGET (throbber), NULL);
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
		g_source_remove (throbber->details->timer_task);
	}
	
	/* reset the frame count */
	throbber->details->current_frame = 0;
	throbber->details->timer_task = g_timeout_add_full (G_PRIORITY_HIGH,
							    throbber->details->delay,
							    bump_throbber_frame,
							    throbber,
							    NULL);
}

static void
nautilus_throbber_remove_update_callback (NautilusThrobber *throbber)
{
	if (throbber->details->timer_task != 0) {
		g_source_remove (throbber->details->timer_task);
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
		g_object_unref (throbber->details->quiescent_pixbuf);
		throbber->details->quiescent_pixbuf = NULL;
	}

	/* unref all the images in the list, and then let go of the list itself */
	current_entry = throbber->details->image_list;
	while (current_entry != NULL) {
		g_object_unref (current_entry->data);
		current_entry = current_entry->next;
	}
	
	g_list_free (throbber->details->image_list);
	throbber->details->image_list = NULL;
}

static GdkPixbuf *
scale_to_real_size (NautilusThrobber *throbber, GdkPixbuf *pixbuf)
{
	GdkPixbuf *result;
	int size;

	size = gdk_pixbuf_get_height (pixbuf);

	if (throbber->details->small_mode) {
		result = gdk_pixbuf_scale_simple (pixbuf,
						  size * 2 / 3,
						  size * 2 / 3,
						  GDK_INTERP_BILINEAR);
	} else {
		result = g_object_ref (pixbuf);
	}

	return result;
}

static GdkPixbuf *
extract_frame (NautilusThrobber *throbber, GdkPixbuf *grid_pixbuf, int x, int y, int size)
{
	GdkPixbuf *pixbuf, *result;

	if (x + size > gdk_pixbuf_get_width (grid_pixbuf) ||
	    y + size > gdk_pixbuf_get_height (grid_pixbuf)) {
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_subpixbuf (grid_pixbuf,
					   x, y,
					   size, size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	result = scale_to_real_size (throbber, pixbuf);
	g_object_unref (pixbuf);

	return result;
}

/* load all of the images of the throbber sequentially */
static void
nautilus_throbber_load_images (NautilusThrobber *throbber)
{
	int grid_width, grid_height, x, y, size;
	GtkIconInfo *icon_info;
	const char *icon;
	GdkPixbuf *icon_pixbuf, *pixbuf;
	GList *image_list;

	nautilus_throbber_unload_images (throbber);

	/* Load the animation */
	icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
						"gnome-spinner", -1, 0);
	if (icon_info == NULL) {
		g_warning ("Throbber animation not found");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);
	
	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	if (icon_pixbuf == NULL) {
		g_warning ("Could not load the spinner file\n");
		gtk_icon_info_free (icon_info);
		return;
	}
	
	grid_width = gdk_pixbuf_get_width (icon_pixbuf);
	grid_height = gdk_pixbuf_get_height (icon_pixbuf);

	image_list = NULL;
	for (y = 0; y < grid_height; y += size) {
		for (x = 0; x < grid_width ; x += size) {
			pixbuf = extract_frame (throbber, icon_pixbuf, x, y, size);

			if (pixbuf) {
				image_list = g_list_prepend (image_list, pixbuf);
			} else {
				g_warning ("Cannot extract frame from the grid");
			}
		}
	}
	throbber->details->image_list = g_list_reverse (image_list);
	throbber->details->max_frame = g_list_length (throbber->details->image_list);

	gtk_icon_info_free (icon_info);
	g_object_unref (icon_pixbuf);

	/* Load the rest icon */
	icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
						"gnome-spinner-rest", -1, 0);
	if (icon_info == NULL) {
		g_warning ("Throbber rest icon not found\n");
		return;
	}

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	g_return_if_fail (icon != NULL);

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	throbber->details->quiescent_pixbuf = scale_to_real_size (throbber, icon_pixbuf);

	g_object_unref (icon_pixbuf);
	gtk_icon_info_free (icon_info);
}

void
nautilus_throbber_set_small_mode (NautilusThrobber *throbber, gboolean new_mode)
{
	if (new_mode != throbber->details->small_mode) {
		throbber->details->small_mode = new_mode;
		nautilus_throbber_load_images (throbber);

		gtk_widget_queue_resize (GTK_WIDGET (throbber));
	}
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

static void
nautilus_throbber_finalize (GObject *object)
{
	NautilusThrobber *throbber;

	throbber = NAUTILUS_THROBBER (object);

	nautilus_throbber_remove_update_callback (throbber);
	nautilus_throbber_unload_images (throbber);
	
	g_free (throbber->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_throbber_class_init (NautilusThrobberClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);
	
	G_OBJECT_CLASS (class)->finalize = nautilus_throbber_finalize;

	widget_class->expose_event = nautilus_throbber_expose;
	widget_class->size_request = nautilus_throbber_size_request;	
	widget_class->map = nautilus_throbber_map;
	widget_class->get_accessible = nautilus_throbber_get_accessible;
}

static AtkObjectClass *a11y_parent_class = NULL;

static void
nautilus_throbber_accessible_initialize (AtkObject *accessible,
					 gpointer   widget)
{
	atk_object_set_name (accessible, _("throbber"));
	atk_object_set_description (accessible, _("provides visual status"));

	a11y_parent_class->initialize (accessible, widget);
}

static void
nautilus_throbber_accessible_class_init (AtkObjectClass *klass)
{
	a11y_parent_class = g_type_class_peek_parent (klass);

	klass->initialize = nautilus_throbber_accessible_initialize;
}

static void
nautilus_throbber_accessible_image_get_size (AtkImage *image,
					     gint     *width,
					     gint     *height)
{
	GtkWidget *widget;

	widget = GTK_ACCESSIBLE (image)->widget;
	if (!widget) {
		*width = *height = 0;
	} else {
		*width = widget->allocation.width;
		*height = widget->allocation.height;
	}
}

static void
nautilus_throbber_accessible_image_interface_init (AtkImageIface *iface)
{
	iface->get_image_size = nautilus_throbber_accessible_image_get_size;
}

static GType
nautilus_throbber_accessible_get_type (void)
{
        static GType type = 0;

	/* Action interface
	   Name etc. ... */
        if (!type) {
		static const GInterfaceInfo atk_image_info = {
			(GInterfaceInitFunc) nautilus_throbber_accessible_image_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = eel_accessibility_create_derived_type 
			("NautilusThrobberAccessible",
			 GTK_TYPE_IMAGE,
			 nautilus_throbber_accessible_class_init);
		
                g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                             &atk_image_info);
        }

        return type;
}

static AtkObject *
nautilus_throbber_get_accessible (GtkWidget *widget)
{
	AtkObject *accessible;
	
	if ((accessible = eel_accessibility_get_atk_object (widget))) {
		return accessible;
	}
	
	accessible = g_object_new 
		(nautilus_throbber_accessible_get_type (), NULL);
	
	return eel_accessibility_set_atk_object_return (widget, accessible);
}

GtkWidget    *
nautilus_throbber_new (void)
{
	return g_object_new (NAUTILUS_TYPE_THROBBER, NULL);
}
