/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gdk-pixbuf-extensions.h: Routines to augment what's in gdk-pixbuf.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_GDK_PIXBUF_EXTENSIONS_H
#define NAUTILUS_GDK_PIXBUF_EXTENSIONS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libnautilus-extensions/nautilus-art-extensions.h>

#define NAUTILUS_STANDARD_ALPHA_THRESHHOLD 128
#define NAUTILUS_OPACITY_FULL 0
#define NAUTILUS_OPACITY_NONE 255

typedef struct NautilusPixbufLoadHandle NautilusPixbufLoadHandle;
typedef void (* NautilusPixbufLoadCallback) (GnomeVFSResult  error,
					     GdkPixbuf      *pixbuf,
					     gpointer        callback_data);

/* Convenience functions for lists of GdkPixbuf objects. */
void                      nautilus_gdk_pixbuf_list_ref                  (GList                      *pixbuf_list);
void                      nautilus_gdk_pixbuf_list_unref                (GList                      *pixbuf_list);
void                      nautilus_gdk_pixbuf_list_free                 (GList                      *pixbuf_list);



/* Loading a GdkPixbuf with a URI. */
GdkPixbuf *               nautilus_gdk_pixbuf_load                      (const char                 *uri);




/* Same thing async. */
NautilusPixbufLoadHandle *nautilus_gdk_pixbuf_load_async                (const char                 *uri,
									 NautilusPixbufLoadCallback  callback,
									 gpointer                    callback_data);
void                      nautilus_cancel_gdk_pixbuf_load               (NautilusPixbufLoadHandle   *handle);
GdkPixbuf *               nautilus_gdk_pixbuf_scale_down_to_fit         (GdkPixbuf                  *pixbuf,
									 int                         max_width,
									 int                         max_height);
GdkPixbuf *               nautilus_gdk_pixbuf_scale_to_fit              (GdkPixbuf                  *pixbuf,
									 int                         max_width,
									 int                         max_height);
double                    nautilus_gdk_scale_to_fit_factor              (int                         width,
									 int                         height,
									 int                         max_width,
									 int                         max_height,
									 int                        *scaled_width,
									 int                        *scaled_height);


/* return average color values for each component */
void                      nautilus_gdk_pixbuf_average_value             (GdkPixbuf                  *pixbuf,
									 GdkColor                   *result_color);
void                      nautilus_gdk_pixbuf_fill_rectangle_with_color (GdkPixbuf                  *pixbuf,
									 const ArtIRect             *area,
									 guint32                     color);


/* Save a pixbuf to a png file.  Return value indicates succss/TRUE or failure/FALSE */
gboolean                  nautilus_gdk_pixbuf_save_to_file              (const GdkPixbuf            *pixbuf,
									 const char                 *file_name);
void                      nautilus_gdk_pixbuf_ref_if_not_null           (GdkPixbuf                  *pixbuf_or_null);
void                      nautilus_gdk_pixbuf_unref_if_not_null         (GdkPixbuf                  *pixbuf_or_null);
/* Copy a pixbuf to an area of a GdkDrawable */
void                      nautilus_gdk_pixbuf_draw_to_drawable          (const GdkPixbuf            *pixbuf,
									 GdkDrawable                *drawable,
									 GdkGC                      *gc,
									 int                         source_x,
									 int                         source_y,
									 const ArtIRect             *destination_area,
									 GdkRgbDither                dither,
									 GdkPixbufAlphaMode          alpha_compositing_mode,
									 int                         alpha_threshold);
/* Copy a pixbuf to an area of another pixbuf */
void                      nautilus_gdk_pixbuf_draw_to_pixbuf            (const GdkPixbuf            *pixbuf,
									 GdkPixbuf                  *destination_pixbuf,
									 int                         source_x,
									 int                         source_y,
									 const ArtIRect             *destination_area);
/* Composite one pixbuf over another with the given opacity */
void                      nautilus_gdk_pixbuf_draw_to_pixbuf_alpha      (const GdkPixbuf            *pixbuf,
									 GdkPixbuf                  *destination_pixbuf,
									 int                         source_x,
									 int                         source_y,
									 const ArtIRect             *destination_area,
									 int                         opacity,
									 GdkInterpType               interpolation_mode);
/* Fill an area of a pixbuf with a tile. */
void                      nautilus_gdk_pixbuf_draw_to_pixbuf_tiled      (const GdkPixbuf            *pixbuf,
									 GdkPixbuf                  *destination_pixbuf,
									 const ArtIRect             *destination_area,
									 int                         tile_width,
									 int                         tile_height,
									 int                         tile_origin_x,
									 int                         tile_origin_y,
									 guchar                      opacity,
									 GdkInterpType               interpolation_mode);

/* Create a pixbuf from a sub area of another pixbuf */
GdkPixbuf *               nautilus_gdk_pixbuf_new_from_pixbuf_sub_area  (GdkPixbuf                  *pixbuf,
									 const ArtIRect             *area);
#endif /* NAUTILUS_GDK_PIXBUF_EXTENSIONS_H */
