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

#ifndef GD_DESKTOP_CANVAS_H
#define GD_DESKTOP_CANVAS_H

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-canvas.h>

BEGIN_GNOME_DECLS


typedef enum {
        DesktopBackgroundImage,
        DesktopBackgroundGradient,
        DesktopBackgroundSolid
} DesktopBackgroundType;

typedef enum {
        DesktopImageTiled,
        DesktopImageCentered,
        DesktopImageScaled,
        DesktopImageScaledAspect /* maintain aspect ratio */
} DesktopImageType;

typedef enum {
        DesktopGradientVertical,
        DesktopGradientHorizontal
} DesktopGradientType;

/* This struct is a convenience thing for passing the desktop
   background info around in a hunk. Not that it's not a union; if you
   change the type, then change it back, the settings for the first type
   are remembered. */
typedef struct _DesktopBackgroundInfo DesktopBackgroundInfo;

struct _DesktopBackgroundInfo {
        guint refcount;
        DesktopBackgroundType background_type;
        DesktopImageType      image_type;
        gchar*                image_filename; /* may be NULL */
        DesktopGradientType   gradient_type;
        guint32               northwest_gradient_color;
        guint32               southeast_gradient_color;
        guint32               solid_color;
};

DesktopBackgroundInfo* desktop_background_info_new   (void);
void                   desktop_background_info_unref (DesktopBackgroundInfo *info);
void                   desktop_background_info_ref   (DesktopBackgroundInfo *info);

/* For everything but refcounting just access the struct directly, the
   image_filename is allocated if non-NULL so g_free() before you replace it */

#define DESKTOP_TYPE_CANVAS            (desktop_canvas_get_type ())
#define DESKTOP_CANVAS(obj)            (GTK_CHECK_CAST ((obj), DESKTOP_TYPE_CANVAS, DesktopCanvas))
#define DESKTOP_CANVAS_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), DESKTOP_TYPE_CANVAS, DesktopCanvasClass))
#define DESKTOP_IS_CANVAS(obj)         (GTK_CHECK_TYPE ((obj), DESKTOP_TYPE_CANVAS))
#define DESKTOP_IS_CANVAS_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), DESKTOP_TYPE_CANVAS))


typedef struct _DesktopCanvas DesktopCanvas;
typedef struct _DesktopCanvasClass DesktopCanvasClass;

struct _DesktopCanvas {
        GnomeCanvas canvas;

        DesktopBackgroundInfo *background_info;        
};

struct _DesktopCanvasClass {
	GnomeCanvasClass parent_class;
};


/* Standard Gtk function */
GtkType desktop_canvas_get_type (void);

GtkWidget *desktop_canvas_new (void);


/* The get/set functions here are not tied to our IDL interface or to
   GConf; these get/set functions are purely to update this "view"
   they do not control the "model" */
void                  desktop_canvas_set_background_type          (DesktopCanvas         *canvas,
                                                                   DesktopBackgroundType  type);
DesktopBackgroundType desktop_canvas_get_background_type          (DesktopCanvas         *canvas);
DesktopImageType      desktop_canvas_get_image_background_type    (DesktopCanvas         *canvas);
void                  desktop_canvas_set_image_background_type    (DesktopCanvas         *canvas,
                                                                   DesktopImageType       type);
void                  desktop_canvas_set_image_background_file    (DesktopCanvas         *canvas,
                                                                   const gchar           *filename);


/* returns allocated string or NULL for unset */
gchar*                desktop_canvas_get_image_background_file    (DesktopCanvas         *canvas);


/* Colors are in 24-bit RGB format, in the rightmost bits */
guint32               desktop_canvas_get_solid_background_color   (DesktopCanvas         *canvas);
void                  desktop_canvas_set_solid_background_color   (DesktopCanvas         *canvas,
                                                                   guint32                rgb);
DesktopGradientType   desktop_canvas_get_gradient_background_type (DesktopCanvas         *canvas);
void                  desktop_canvas_set_gradient_background_type (DesktopCanvas         *canvas,
                                                                   DesktopGradientType    type);
guint32               desktop_canvas_get_northwest_gradient_color (DesktopCanvas         *canvas);
void                  desktop_canvas_set_northwest_gradient_color (DesktopCanvas         *canvas,
                                                                   guint32                rgb);
guint32               desktop_canvas_get_southeast_gradient_color (DesktopCanvas         *canvas);
void                  desktop_canvas_set_southeast_gradient_color (DesktopCanvas         *canvas,
                                                                   guint32                rgb);

END_GNOME_DECLS

#endif
