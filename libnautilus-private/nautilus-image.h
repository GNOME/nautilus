/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-image.h - A widget to smoothly display images.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_IMAGE_H
#define NAUTILUS_IMAGE_H

#include <gtk/gtkmisc.h>
#include <libgnome/gnome-defs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rect.h>
#include <libnautilus-extensions/nautilus-smooth-widget.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_IMAGE            (nautilus_image_get_type ())
#define NAUTILUS_IMAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_IMAGE, NautilusImage))
#define NAUTILUS_IMAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_IMAGE, NautilusImageClass))
#define NAUTILUS_IS_IMAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_IMAGE))
#define NAUTILUS_IS_IMAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_IMAGE))

typedef struct _NautilusImage	       NautilusImage;
typedef struct _NautilusImageClass     NautilusImageClass;
typedef struct _NautilusImageDetails   NautilusImageDetails;

struct _NautilusImage
{
	/* Superclass */
	GtkMisc misc;

	/* Private things */
	NautilusImageDetails *details;
};

struct _NautilusImageClass
{
	GtkMiscClass parent_class;

	NautilusSmoothWidgetDrawBackground draw_background;
	NautilusSmoothWidgetSetIsSmooth set_is_smooth;
};

GtkType                      nautilus_image_get_type                       (void);
GtkWidget *                  nautilus_image_new                            (const char                   *file_name);
GtkWidget *                  nautilus_image_new_solid                      (GdkPixbuf                    *pixbuf,
									    float                         xalign,
									    float                         yalign,
									    int                           xpadding,
									    int                           ypadding,
									    guint32                       background_color,
									    GdkPixbuf                    *tile_pixbuf);
void                         nautilus_image_set_is_smooth                  (NautilusImage                *image,
									    gboolean                      is_smooth);
gboolean                     nautilus_image_get_is_smooth                  (const NautilusImage          *image);
void                         nautilus_image_set_tile_pixbuf                (NautilusImage                *image,
									    GdkPixbuf                    *pixbuf);
void                         nautilus_image_set_tile_width                 (NautilusImage                *image,
									    int                           tile_width);
int                          nautilus_image_get_tile_width                 (const NautilusImage          *image);
void                         nautilus_image_set_tile_height                (NautilusImage                *image,
									    int                           tile_height);
int                          nautilus_image_get_tile_height                (const NautilusImage          *image);
void                         nautilus_image_set_tile_pixbuf_from_file_name (NautilusImage                *image,
									    const char                   *tile_file_name);
GdkPixbuf*                   nautilus_image_get_tile_pixbuf                (const NautilusImage          *image);
void                         nautilus_image_set_tile_opacity               (NautilusImage                *image,
									    int                           tile_opacity);
int                          nautilus_image_get_tile_opacity               (const NautilusImage          *image);
void                         nautilus_image_set_tile_mode_vertical         (NautilusImage                *image,
									    NautilusSmoothTileMode        horizontal_tile_mode);
NautilusSmoothTileMode       nautilus_image_get_tile_mode_vertical         (const NautilusImage          *image);
void                         nautilus_image_set_tile_mode_horizontal       (NautilusImage                *image,
									    NautilusSmoothTileMode        horizontal_tile_mode);
NautilusSmoothTileMode       nautilus_image_get_tile_mode_horizontal       (const NautilusImage          *image);
void                         nautilus_image_set_pixbuf                     (NautilusImage                *image,
									    GdkPixbuf                    *pixbuf);
void                         nautilus_image_set_pixbuf_from_file_name      (NautilusImage                *image,
									    const char                   *file_name);
GdkPixbuf*                   nautilus_image_get_pixbuf                     (const NautilusImage          *image);
void                         nautilus_image_set_pixbuf_opacity             (NautilusImage                *image,
									    int                           pixbuf_opacity);
int                          nautilus_image_get_pixbuf_opacity             (const NautilusImage          *image);
void                         nautilus_image_set_background_mode            (NautilusImage                *image,
									    NautilusSmoothBackgroundMode  background_mode);
NautilusSmoothBackgroundMode nautilus_image_get_background_mode            (const NautilusImage          *image);
void                         nautilus_image_set_solid_background_color     (NautilusImage                *image,
									    guint32                       solid_background_color);
guint32                      nautilus_image_get_solid_background_color     (const NautilusImage          *image);
void                         nautilus_image_set_never_smooth               (NautilusImage                *image,
									    gboolean                      never_smooth);

END_GNOME_DECLS

#endif /* NAUTILUS_IMAGE_H */


