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
#include "desktop-menu.h"

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
desktop_canvas_init (DesktopCanvas *dcanvas)
{
        dcanvas->background_info = desktop_background_info_new();
        dcanvas->background_update_idle = 0;

        dcanvas->popup = desktop_menu_new();

        /* the attachment holds a refcount and eventually destroys
           the popup */
        gnome_popup_menu_attach(dcanvas->popup, dcanvas, NULL);
}

static void
desktop_canvas_destroy (GtkObject *object)
{
        DesktopCanvas *canvas;

        g_return_if_fail(object != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(object));

        canvas = DESKTOP_CANVAS(object);

        if (canvas->background_update_idle != 0) {
                gtk_idle_remove(canvas->background_update_idle);
                canvas->background_update_idle = 0;
        }
                
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

static void
rgb32_to_gdkcolor(guint32 rgb, GdkColor* color)
{
        guchar r, g, b;

        r = (rgb >> 16) & 0xFF;
        g = (rgb >> 8) & 0xFF;
        b = rgb & 0xFF;

        color->red = r*255;
        color->green = g*255;
        color->blue = b*255;
}

static void
set_widget_color(GtkWidget* w, guint32 rgb)
{
        GtkStyle* style;
        
        gtk_widget_ensure_style(w);

        style = gtk_style_copy(w->style);

        rgb32_to_gdkcolor(rgb, &style->bg[GTK_STATE_NORMAL]);

        /* FIXME should use modify_style */
        gtk_widget_set_style(w, style);
        
        gtk_style_unref(style);
}

static gint
update_bg_idle(gpointer data)
{
        DesktopCanvas *canvas;

        canvas = data;

        g_return_val_if_fail(canvas != NULL, FALSE);
        g_return_val_if_fail(DESKTOP_IS_CANVAS(canvas), FALSE);

        switch (canvas->background_info->background_type) {
        case DesktopBackgroundSolid:
                set_widget_color(GTK_WIDGET(canvas), canvas->background_info->solid_color);
                break;
        default:
                g_warning("FIXME background features not all implemented yet");
                break;
        }
                
        canvas->background_update_idle = 0;
        return FALSE; /* remove idle */
}

/* This function is expensive. All the "setters" check that a change has
   actually been made before they call it. */
static void
desktop_canvas_update_background            (DesktopCanvas         *canvas)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_update_idle == 0)
                canvas->background_update_idle = gtk_idle_add(update_bg_idle, canvas);
}

void
desktop_canvas_set_background_type          (DesktopCanvas         *canvas,
                                             DesktopBackgroundType  type)
{
        g_return_if_fail(canvas != NULL);
        g_return_if_fail(DESKTOP_IS_CANVAS(canvas));

        if (canvas->background_info->background_type == type)
                return;
        
        canvas->background_info->background_type = type;

        desktop_canvas_update_background (canvas);
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

        desktop_canvas_update_background (canvas);
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

        desktop_canvas_update_background (canvas);
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

        desktop_canvas_update_background (canvas);
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

        desktop_canvas_update_background (canvas);
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

        desktop_canvas_update_background (canvas);
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

        desktop_canvas_update_background (canvas);
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




