/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Desktop component of GNOME file manager
 * 
 * Copyright (C) 1999 Red Hat Inc., Free Software Foundation
 * (based on Midnight Commander code by Federico Mena Quintero and Miguel de Icaza)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "desktop-canvas.h"
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-canvas-text.h>
#include <gtk/gtk.h>

static void desktop_canvas_class_init (DesktopCanvasClass *class);
static void desktop_canvas_init       (DesktopCanvas      *dcanvas);
static void desktop_canvas_realize    (GtkWidget        *widget);
static void desktop_canvas_size_allocate(GtkWidget        *widget,
                                         GtkAllocation    *allocation);

static void desktop_canvas_destroy    (GtkObject          *object);
static void desktop_canvas_finalize   (GtkObject          *object);

static GnomeCanvasClass *parent_class;


/**
 * desktop_canvas_get_type
 *
 * Returns the Gtk type assigned to the DesktopCanvas class.
 */
GtkType
desktop_canvas_get_type (void)
{
	static GtkType desktop_canvas_type = 0;

	if (!desktop_canvas_type) {
		GtkTypeInfo desktop_canvas_info = {
			"DesktopCanvas",
			sizeof (DesktopCanvas),
			sizeof (DesktopCanvasClass),
			(GtkClassInitFunc) desktop_canvas_class_init,
			(GtkObjectInitFunc) desktop_canvas_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		desktop_canvas_type = gtk_type_unique (gnome_canvas_get_type (), &desktop_canvas_info);
	}

	return desktop_canvas_type;
}

static void
desktop_canvas_class_init (DesktopCanvasClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = gtk_type_class (gnome_canvas_get_type ());

        object_class->destroy = desktop_canvas_destroy;
        object_class->finalize = desktop_canvas_finalize;
        
	widget_class->realize = desktop_canvas_realize;
        widget_class->size_allocate = desktop_canvas_size_allocate;
}

static void
event_cb(GnomeCanvasItem* item, GdkEvent* event, gpointer data)
{
        /* Debug */
        if (event->type == GDK_BUTTON_PRESS)
                gtk_main_quit();

}


static gint
item_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	static double x, y;
	double new_x, new_y;
	GdkCursor *fleur;
	static int dragging;
	double item_x, item_y;

	/* set item_[xy] to the event x,y position in the parent's item-relative coordinates */
	item_x = event->button.x;
	item_y = event->button.y;
	gnome_canvas_item_w2i (item->parent, &item_x, &item_y);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (event->button.button) {
		case 1:
			if (event->button.state & GDK_SHIFT_MASK)
				gtk_object_destroy (GTK_OBJECT (item));
			else {
				x = item_x;
				y = item_y;

				fleur = gdk_cursor_new (GDK_FLEUR);
				gnome_canvas_item_grab (item,
							GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
							fleur,
							event->button.time);
				gdk_cursor_destroy (fleur);
				dragging = TRUE;
			}
			break;

		case 2:
			if (event->button.state & GDK_SHIFT_MASK)
				gnome_canvas_item_lower_to_bottom (item);
			else
				gnome_canvas_item_lower (item, 1);
			break;

		case 3:
			if (event->button.state & GDK_SHIFT_MASK)
				gnome_canvas_item_raise_to_top (item);
			else
				gnome_canvas_item_raise (item, 1);
			break;

		default:
			break;
		}

		break;

	case GDK_MOTION_NOTIFY:
		if (dragging && (event->motion.state & GDK_BUTTON1_MASK)) {
			new_x = item_x;
			new_y = item_y;

			gnome_canvas_item_move (item, new_x - x, new_y - y);
			x = new_x;
			y = new_y;
		}
		break;

	case GDK_BUTTON_RELEASE:
		gnome_canvas_item_ungrab (item, event->button.time);
		dragging = FALSE;
		break;

	default:
		break;
	}

	return FALSE;
}

static void
setup_item (GnomeCanvasItem *item)
{
	gtk_signal_connect (GTK_OBJECT (item), "event",
			    (GtkSignalFunc) item_event,
			    NULL);
}

#define gray50_width 2
#define gray50_height 2
static char gray50_bits[] = {
  0x02, 0x01, };

static void
desktop_canvas_init (DesktopCanvas *dcanvas)
{
        GnomeCanvasItem* ellipse;
        GnomeCanvasItem* rect;
        GnomeCanvasItem* text;
        double x, y;
        GdkBitmap* stipple;
        gboolean use_stipple = FALSE;

        dcanvas->background_info = desktop_background_info_new();
        
        stipple = gdk_bitmap_create_from_data (NULL, gray50_bits, gray50_width, gray50_height);
        
        rect = gnome_canvas_item_new ((GnomeCanvasGroup*)GNOME_CANVAS(dcanvas)->root,
                                      gnome_canvas_rect_get_type (),
                                      "x1", 0.0,
                                      "y1", 0.0,
                                      "x2", (double)gdk_screen_width(),
                                      "y2", (double)gdk_screen_height(),
                                      "outline_color", "red",
                                      "fill_color", "blue", 
                                      "width_pixels", 8,
                                      NULL);

        x = gdk_screen_width();
        y = 0.0;

        while (x > 0.0 || y < gdk_screen_height()) {
        
                rect = gnome_canvas_item_new ((GnomeCanvasGroup*)GNOME_CANVAS(dcanvas)->root,
                                              gnome_canvas_rect_get_type (),
                                              "x1", x - 103.0,
                                              "y1", y,
                                              "x2", x,
                                              "y2", y + 70.0,
                                              "fill_color", "red",
                                              "fill_stipple",
                                              use_stipple ? stipple : NULL,
                                              NULL);

                setup_item(rect);

                use_stipple = !use_stipple;
                
                x -= 105;
                y += 80;
        }
        
        x = 0.0;
        y = 0.0;

        while (x < gdk_screen_width() || y < gdk_screen_height()) {
        
                rect = gnome_canvas_item_new ((GnomeCanvasGroup*)GNOME_CANVAS(dcanvas)->root,
                                              gnome_canvas_rect_get_type (),
                                              "x1", x,
                                              "y1", y,
                                              "x2", x + 53.0,
                                              "y2", y + 30.0,
                                              "fill_color", "green",
                                              "fill_stipple",
                                              use_stipple ? stipple : NULL,
                                              NULL);

                setup_item(rect);

                use_stipple = !use_stipple;
                
                x += 90;
                y += 80;
        }

        gdk_bitmap_unref (stipple);
        
        ellipse =
                gnome_canvas_item_new ((GnomeCanvasGroup*)GNOME_CANVAS(dcanvas)->root,
                                       gnome_canvas_ellipse_get_type (),
                                       "x1", 220.0,
                                       "y1", 30.0,
                                       "x2", 270.0,
                                       "y2", 60.0,
                                       "outline_color", "goldenrod",
                                       "width_pixels", 8,
                                       NULL);

        gtk_signal_connect(GTK_OBJECT(ellipse), "event",
                           GTK_SIGNAL_FUNC(event_cb), NULL);
        

        text = gnome_canvas_item_new ((GnomeCanvasGroup*)GNOME_CANVAS(dcanvas)->root,
                                       gnome_canvas_text_get_type (),
                                      "x", 300.0,
                                      "y", 70.0,
                                      "text", "click ellipse to exit",
                                      "font", "fixed",
                                      "anchor", GTK_ANCHOR_NORTH_WEST,
                                      "fill_color", "black",
                                      NULL);

        setup_item(text);
}


static void
desktop_canvas_destroy (GtkObject *object)
{
        DesktopCanvas *canvas;

        g_return_if_fail(object != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(object));

        canvas = DESKTOP_CANVAS(object);

        /* does nothing for now */

        (*  GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

static void
desktop_canvas_finalize (GtkObject *object)
{
        DesktopCanvas *canvas;

        g_return_if_fail(object != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(object));

        canvas = DESKTOP_CANVAS(object);

        desktop_background_info_unref(canvas->background_info);

        (* GTK_OBJECT_CLASS(parent_class)->finalize) (object);
}


GtkWidget*
desktop_canvas_new (void)
{
        DesktopCanvas *dcanvas;

        dcanvas = gtk_type_new(desktop_canvas_get_type());

        return GTK_WIDGET(dcanvas);
}

/*
 * GtkWidget functions
 */

static void
desktop_canvas_realize (GtkWidget *widget)
{
	DesktopCanvas *dcanvas;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (DESKTOP_IS_CANVAS (widget));

	dcanvas = DESKTOP_CANVAS (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
}

static void
desktop_canvas_size_allocate(GtkWidget        *widget,
                             GtkAllocation    *allocation)
{
        gnome_canvas_set_scroll_region(GNOME_CANVAS(widget),
                                       0.0, 0.0,
                                       allocation->width,
                                       allocation->height);

        if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget,
                                                                    allocation);
}

/*
 * Background accessor functions
 */

void
desktop_canvas_set_background_type          (DesktopCanvas         *canvas,
                                             DesktopBackgroundType  type)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->background_type == type)
                return;
        
        canvas->background_info->background_type = type;
}

DesktopBackgroundType
desktop_canvas_get_background_type          (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, DesktopBackgroundSolid);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), DesktopBackgroundSolid);

        return canvas->background_info->background_type;
}

DesktopImageType
desktop_canvas_get_image_background_type    (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, DesktopImageCentered);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), DesktopImageCentered);

        return canvas->background_info->image_type;
}

void
desktop_canvas_set_image_background_type    (DesktopCanvas         *canvas,
                                             DesktopImageType       type)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->image_type == type)
                return;

        canvas->background_info->image_type = type;
}

void
desktop_canvas_set_image_background_file    (DesktopCanvas         *canvas,
                                             const gchar           *filename)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (filename == NULL &&
            canvas->background_info->image_filename == NULL)
                return;

        if (filename != NULL &&
            canvas->background_info->image_filename != NULL &&
            strcmp(canvas->background_info->image_filename, filename) == 0)
                return;

        if (canvas->background_info->image_filename)
                g_free(canvas->background_info->image_filename);

        canvas->background_info->image_filename = filename ? g_strdup(filename) : NULL;
}


gchar*
desktop_canvas_get_image_background_file    (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, NULL);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), NULL);

        if (canvas->background_info->image_filename)
                return g_strdup(canvas->background_info->image_filename);
        else
                return NULL;
}


guint32
desktop_canvas_get_solid_background_color   (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, 0);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), 0);

        return canvas->background_info->solid_color;
}

void
desktop_canvas_set_solid_background_color   (DesktopCanvas         *canvas,
                                             guint32                rgb)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->solid_color == rgb)
                return;

        canvas->background_info->solid_color = rgb;
}

DesktopGradientType
desktop_canvas_get_gradient_background_type (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, DesktopGradientVertical);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), DesktopGradientVertical);

        return canvas->background_info->gradient_type;
}

void
desktop_canvas_set_gradient_background_type (DesktopCanvas         *canvas,
                                             DesktopGradientType    type)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->gradient_type == type)
                return;

        canvas->background_info->gradient_type = type;
}

guint32
desktop_canvas_get_northwest_gradient_color (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, 0);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), 0);

        return canvas->background_info->northwest_gradient_color;
}

void
desktop_canvas_set_northwest_gradient_color (DesktopCanvas         *canvas,
                                             guint32                rgb)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->northwest_gradient_color == rgb)
                return;

        canvas->background_info->northwest_gradient_color = rgb;
}


guint32
desktop_canvas_get_southeast_gradient_color (DesktopCanvas         *canvas)
{
        g_return_val_if_fail(canvas != NULL, 0);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), 0);

        return canvas->background_info->southeast_gradient_color;
}

void
desktop_canvas_set_southeast_gradient_color (DesktopCanvas         *canvas,
                                             guint32 rgb)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->southeast_gradient_color == rgb)
                return;
        
        canvas->background_info->southeast_gradient_color = rgb;
}


/*
 * DesktopBackgroundInfo
 */

DesktopBackgroundInfo*
desktop_background_info_new (void)
{
        DesktopBackgroundInfo *info;

        info = g_new(DesktopBackgroundInfo, 1);

        info->background_type = DesktopBackgroundSolid;
        info->image_type = DesktopImageCentered;
        info->image_filename = NULL;
        info->gradient_type = DesktopGradientVertical;
        info->northwest_gradient_color = 0x0500AA;
        info->southeast_gradient_color = 0x0000FF;
        info->solid_color = 0x0500AA;

        return info;
}

void
desktop_background_info_unref(DesktopBackgroundInfo *info)
{
        g_return_if_fail(info != NULL);
        g_return_if_fail(info->refcount > 0);

        info->refcount -= 1;

        if (info->refcount == 0) {
                if (info->image_filename)
                        g_free(info->image_filename);

                g_free(info);
        }
}

void
desktop_background_info_ref (DesktopBackgroundInfo *info)
{
        g_return_if_fail(info != NULL);
        g_return_if_fail(info->refcount > 0);
        
        info->refcount += 1;
}




