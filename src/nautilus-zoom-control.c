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

#include <gdk/gdk.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkwidget.h>
#include <stdio.h>
#include <gnome.h>
#include <math.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-icon-factory.h>
#include "nautilus-zoom-control.h"

static void nautilus_zoom_control_class_initialize 	(NautilusZoomControlClass *klass);
static void nautilus_zoom_control_initialize       	(NautilusZoomControl *pixmap);
static void nautilus_zoom_control_draw 			(GtkWidget *widget, GdkRectangle *box);
static gint nautilus_zoom_control_expose 		(GtkWidget *widget, GdkEventExpose *event);
static gboolean nautilus_zoom_control_button_press_event(GtkWidget *widget, GdkEventButton *event);
static void set_zoom_level(NautilusZoomControl *zoom_control, gint new_level);
void draw_number_and_disable_arrows(GtkWidget *widget, GdkRectangle *box);

static GtkEventBoxClass *parent_class;

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

GtkType
nautilus_zoom_control_get_type (void)
{
  static GtkType zoom_control_type = 0;

  if (!zoom_control_type)
    {
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
}

static void
nautilus_zoom_control_initialize (NautilusZoomControl *zoom_control)
{
  gchar *file_name;
  GtkWidget *pix_widget;

  zoom_control->current_zoom = NAUTILUS_ZOOM_LEVEL_STANDARD;
  zoom_control->min_zoom = NAUTILUS_ZOOM_LEVEL_SMALLEST;
  zoom_control->max_zoom = NAUTILUS_ZOOM_LEVEL_LARGEST;
  zoom_control->zoom_factor = 1.0;

  /* allocate the pixmap that holds the image */
  
  file_name = gnome_pixmap_file ("nautilus/zoom.png");
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
  gchar buffer[8];
  GdkFont *label_font;
  GdkGC* temp_gc; 
  gint x, y, percent; 
  NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
 
  label_font = gdk_font_load("-bitstream-courier-medium-r-normal-*-9-*-*-*-*-*-*-*");
  temp_gc = gdk_gc_new(widget->window);

  percent = floor((100.0 * zoom_control->zoom_factor) + .5);
  g_snprintf(buffer, 8, "%d", percent);
  
  x = (box->width - gdk_string_width(label_font, buffer)) >> 1;  
  y = (box->height >> 1) + 3;
    
  gdk_draw_string (widget->window, label_font, temp_gc, x, y, &buffer[0]);
  gdk_font_unref(label_font);
  
  /* clear the arrows if necessary */
  
  if (zoom_control->current_zoom == zoom_control->min_zoom)
    gdk_draw_rectangle (widget->window, widget->style->bg_gc[0], TRUE, 0, 0, box->width / 4, box->height);
  else if (zoom_control->current_zoom == zoom_control->max_zoom)
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

static gint
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
zoom_menu_cb(GtkMenuItem *item, gpointer data)
{
  NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL(gtk_object_get_user_data(GTK_OBJECT(item)));
  if (zoom_control)
      set_zoom_level(zoom_control, GPOINTER_TO_INT(data));
}

static void
create_zoom_menu_item(GtkMenu* menu, GtkWidget *zoom_control, const gchar *item_text, gint zoom_index)
{
  GtkWidget *menu_item;
  
  menu_item = gtk_menu_item_new_with_label(item_text);
  gtk_signal_connect(GTK_OBJECT (menu_item), "activate", GTK_SIGNAL_FUNC (zoom_menu_cb), GINT_TO_POINTER(zoom_index));

  gtk_object_set_user_data(GTK_OBJECT(menu_item), zoom_control);
  gtk_widget_show (menu_item);
  gtk_menu_append (menu, menu_item);
}

static GtkMenu*
create_zoom_menu(GtkWidget *zoom_control)
{
  GtkMenu *menu = GTK_MENU (gtk_menu_new ());

  create_zoom_menu_item(menu, zoom_control, "25%",  NAUTILUS_ZOOM_LEVEL_SMALLEST);
  create_zoom_menu_item(menu, zoom_control, "50%",  NAUTILUS_ZOOM_LEVEL_SMALLER);
  create_zoom_menu_item(menu, zoom_control, "75%",  NAUTILUS_ZOOM_LEVEL_SMALL);
  create_zoom_menu_item(menu, zoom_control, "100%", NAUTILUS_ZOOM_LEVEL_STANDARD);
  create_zoom_menu_item(menu, zoom_control, "150%",  NAUTILUS_ZOOM_LEVEL_LARGE);
  create_zoom_menu_item(menu, zoom_control, "200%", NAUTILUS_ZOOM_LEVEL_LARGER);
  create_zoom_menu_item(menu, zoom_control, "400%", NAUTILUS_ZOOM_LEVEL_LARGEST);

  return menu;  
}
  
static void
set_zoom_level(NautilusZoomControl *zoom_control, gint new_level)
{
  gtk_widget_queue_draw (GTK_WIDGET (zoom_control));	
  zoom_control->current_zoom = new_level;
  zoom_control->zoom_factor = (double) nautilus_get_icon_size_for_zoom_level
	  (zoom_control->current_zoom) / NAUTILUS_ICON_SIZE_STANDARD;    
  /* FIXME: tell the content view about the zoom change here soon */
}

/* handle button presses */

static gboolean
nautilus_zoom_control_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  NautilusZoomControl *zoom_control = NAUTILUS_ZOOM_CONTROL (widget);
  gint width = widget->allocation.width;
  gint center = width >> 1;
  gint changed = FALSE;

  /* check for the context menu button and handle by creating and showing the menu */  
  if (event->button == CONTEXTUAL_MENU_BUTTON)
    {
 	  GtkMenu *zoom_menu = create_zoom_menu(widget);
	  
	  gtk_object_ref (GTK_OBJECT(zoom_menu));
	  gtk_object_sink (GTK_OBJECT(zoom_menu));

	  gtk_menu_popup (zoom_menu, NULL, NULL, NULL, NULL, 3, GDK_CURRENT_TIME);
	  gtk_object_unref (GTK_OBJECT(zoom_menu));
	  return TRUE;	  
 	}
 	  
  if (event->x < (width / 3) && (zoom_control->current_zoom > zoom_control->min_zoom))
    {
	  zoom_control->current_zoom -= 1;
	  changed = TRUE;
	}
  else if ((event->x > ((2 * width) / 3)) && (zoom_control->current_zoom < zoom_control->max_zoom))
	{
	  zoom_control->current_zoom += 1;
	  changed = TRUE;
	}
  else if ((event->x >= (center - (width >> 3))) && (event->x <= (center + (width >> 3))))
    {
	  zoom_control->current_zoom = NAUTILUS_ZOOM_LEVEL_STANDARD;
	  changed = TRUE;
    }
    
  if (changed)
    {
	  set_zoom_level(zoom_control, zoom_control->current_zoom);
    }	  
  
  return TRUE;
}
