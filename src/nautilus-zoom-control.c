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
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

enum {
	ZOOM_IN,
	ZOOM_OUT,
	ZOOM_TO_LEVEL,
	ZOOM_DEFAULT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void     nautilus_zoom_control_class_initialize 	 (NautilusZoomControlClass *klass);
static void     nautilus_zoom_control_initialize       	 (NautilusZoomControl *pixmap);
static void     nautilus_zoom_control_draw 	       	 (GtkWidget *widget, 
							  GdkRectangle *box);
static int     nautilus_zoom_control_expose 		 (GtkWidget *widget, 
							  GdkEventExpose *event);
static gboolean nautilus_zoom_control_button_press_event (GtkWidget *widget, 
							  GdkEventButton *event);
void            draw_number_and_disable_arrows           (GtkWidget *widget, 
							  GdkRectangle *box);

static GtkEventBoxClass *parent_class;

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

GtkType
nautilus_zoom_control_get_type (void)
{
	static GtkType zoom_control_type = 0;
	
	if (!zoom_control_type) {
		static const GtkTypeInfo zoom_control_info =
		{
			"NautilusZoomControl",
			sizeof (NautilusZoomControl),
			sizeof (NautilusZoomControlClass),
			(GtkClassInitFunc) nautilus_zoom_control_class_initialize,
			(GtkObjectInitFunc) nautilus_zoom_control_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		zoom_control_type = gtk_type_unique (gtk_event_box_get_type(), &zoom_control_info);
	}
	
	return zoom_control_type;
}

static void
nautilus_zoom_control_class_initialize (NautilusZoomControlClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	
	widget_class->draw = nautilus_zoom_control_draw;
	widget_class->expose_event = nautilus_zoom_control_expose;
	widget_class->button_press_event = nautilus_zoom_control_button_press_event;

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

	signals[ZOOM_DEFAULT] =
		gtk_signal_new ("zoom_default",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusZoomControlClass, 
						   zoom_default),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
  

}

static void
nautilus_zoom_control_initialize (NautilusZoomControl *zoom_control)
{
	char *file_name;
	GtkWidget *pix_widget;
	
	zoom_control->zoom_level = 1.0;
	zoom_control->min_zoom_level = 0.0;
	zoom_control->max_zoom_level = 4.0;
	
	/* allocate the pixmap that holds the image */
	
	file_name = nautilus_pixmap_file ("zoom.png");
	pix_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_widget_show (pix_widget);
	gtk_container_add (GTK_CONTAINER(zoom_control), pix_widget);
	g_free (file_name);

}

GtkWidget*
nautilus_zoom_control_new ()
{
	NautilusZoomControl *zoom_control = gtk_type_new (nautilus_zoom_control_get_type ());
	return GTK_WIDGET (zoom_control);
}

/* draw the current zoom percentage */
void draw_number_and_disable_arrows(GtkWidget *widget, GdkRectangle *box)
{
	char buffer[8];
	GdkFont *label_font;
	GdkGC* temp_gc; 
	int x, y, percent; 
	NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
	
	label_font = gdk_font_load("-bitstream-courier-medium-r-normal-*-9-*-*-*-*-*-*-*");
	temp_gc = gdk_gc_new(widget->window);
	
	percent = floor((100.0 * zoom_control->zoom_level) + .5);
	g_snprintf(buffer, 8, "%d", percent);
	
	x = (box->width - gdk_string_width(label_font, buffer)) >> 1;  
	y = (box->height >> 1) + 3;
	
	gdk_draw_string (widget->window, label_font, temp_gc, x, y, &buffer[0]);
	gdk_font_unref(label_font);
	
	/* clear the arrows if necessary */
	
	if (zoom_control->zoom_level <= zoom_control->min_zoom_level)
		gdk_draw_rectangle (widget->window, widget->style->bg_gc[0], TRUE, 0, 0, box->width / 4, box->height);
	else if (zoom_control->zoom_level >= zoom_control->max_zoom_level)
		gdk_draw_rectangle (widget->window, widget->style->bg_gc[0], TRUE, box->width - (box->width / 4), 0, box->width / 4, box->height);
	
	gdk_gc_unref(temp_gc);
}

static void
nautilus_zoom_control_draw (GtkWidget *widget, GdkRectangle *box)
{ 
	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_ZOOM_CONTROL (widget));
	
	/* invoke our superclass to draw the image */
 	
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, draw, (widget, box));
	
	draw_number_and_disable_arrows(widget, box);
}

/* handle expose events */

static int
nautilus_zoom_control_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GdkRectangle box;
	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_ZOOM_CONTROL (widget), FALSE);
	
	/* invoke our superclass to draw the image */
 	
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, expose_event, (widget, event));

	box.x = 0; box.y = 0;
	box.width = widget->allocation.width;
	box.height = widget->allocation.height;
	
	draw_number_and_disable_arrows(widget, &box);
	
	return FALSE;
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
	if ((zoom_control->min_zoom_level <= zoom_level && zoom_level <  zoom_control->zoom_level) ||
	    (zoom_control->zoom_level     < zoom_level && zoom_level  <= zoom_control->max_zoom_level)) {
		gtk_signal_emit (GTK_OBJECT (zoom_control), signals[ZOOM_TO_LEVEL], zoom_level);
	}
}

static void
create_zoom_menu_item (GtkMenu *menu, GtkWidget *zoom_control,
		       const char *item_text, double zoom_level)
{
	GtkWidget *menu_item;
	double	  *zoom_level_ptr;
  
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
	GtkMenu *menu = GTK_MENU (gtk_menu_new ());
	
	create_zoom_menu_item(menu, zoom_control, _("25%"),  (double) NAUTILUS_ICON_SIZE_SMALLEST / NAUTILUS_ICON_SIZE_STANDARD);
	create_zoom_menu_item(menu, zoom_control, _("50%"),  (double) NAUTILUS_ICON_SIZE_SMALLER / NAUTILUS_ICON_SIZE_STANDARD);
	create_zoom_menu_item(menu, zoom_control, _("75%"),  (double) NAUTILUS_ICON_SIZE_SMALL / NAUTILUS_ICON_SIZE_STANDARD);
	create_zoom_menu_item(menu, zoom_control, _("100%"), (double) NAUTILUS_ICON_SIZE_STANDARD / NAUTILUS_ICON_SIZE_STANDARD);
	create_zoom_menu_item(menu, zoom_control, _("150%"), (double) NAUTILUS_ICON_SIZE_LARGE / NAUTILUS_ICON_SIZE_STANDARD);
	create_zoom_menu_item(menu, zoom_control, _("200%"), (double) NAUTILUS_ICON_SIZE_LARGER / NAUTILUS_ICON_SIZE_STANDARD);
	create_zoom_menu_item(menu, zoom_control, _("400%"), (double) NAUTILUS_ICON_SIZE_LARGEST / NAUTILUS_ICON_SIZE_STANDARD);
	
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
		GtkMenu *zoom_menu = create_zoom_menu(widget);
		
		gtk_object_ref (GTK_OBJECT(zoom_menu));
		gtk_object_sink (GTK_OBJECT(zoom_menu));
		
		gtk_menu_popup (zoom_menu, NULL, NULL, NULL, NULL, 3, GDK_CURRENT_TIME);
		gtk_object_unref (GTK_OBJECT(zoom_menu));
		return TRUE;	  
 	}
	
	if (event->x < (width / 3) && (zoom_control->zoom_level > zoom_control->min_zoom_level)) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_OUT]);			
	} else if ((event->x > ((2 * width) / 3)) && (zoom_control->zoom_level < zoom_control->max_zoom_level)) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_IN]);			
	} else if ((event->x >= (center - (width >> 3))) && (event->x <= (center + (width >> 3)))) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[ZOOM_DEFAULT]);			
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

/*
 * FIXME bugzila.eazel.com 1425:
 * 
 * The term zoom level is overloaded in our sources:
 * 
 * The zoom control function APIs (e.g. nautilus_zoom_control_set_zoom_level)
 * uses doubles for zoom level (e.g. 1.5 = 150% zoom). This matches nicely with
 * nautilus-zoomable interface which uses doubles for zoom level too.
 * 
 * However, internally the zoom control keeps state (current_zoom) in terms of
 * NautilusZoomLevel's which are defined in nautilus-icon-factory.h as the 7
 * discrete icon sizes.
 * 
 * Furthermore, the signals the zoom control emits, specifically zoom_to_level,
 * passes a zoom level that's one of the NautilusZoomLevel's.
 * 
 * It would probably be cleaner to redo the zoom control APIs to all work on
 * doubles.
 */
 

void
nautilus_zoom_control_set_zoom_level (NautilusZoomControl *zoom_control, double zoom_level)
{
	zoom_control->zoom_level = zoom_level;
	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
}

void
nautilus_zoom_control_set_min_zoom_level (NautilusZoomControl *zoom_control, double  zoom_level)
{
	zoom_control->min_zoom_level = zoom_level;
	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
}

void
nautilus_zoom_control_set_max_zoom_level (NautilusZoomControl *zoom_control, double zoom_level)
{
	zoom_control->max_zoom_level = zoom_level;
	gtk_widget_queue_draw (GTK_WIDGET (zoom_control));
}

double
nautilus_zoom_control_get_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->zoom_level;
}

double
nautilus_zoom_control_get_min_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->min_zoom_level;
}

double
nautilus_zoom_control_get_max_zoom_level (NautilusZoomControl *zoom_control)
{
	return zoom_control->max_zoom_level;
}


