/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-pixbuf-extensions.h: Routines to augment what's in gdk-pixbuf.

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

#ifndef EEL_GDK_PIXBUF_EXTENSIONS_H
#define EEL_GDK_PIXBUF_EXTENSIONS_H

#include <eel/eel-art-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

#define EEL_STANDARD_ALPHA_THRESHHOLD 128
#define EEL_OPACITY_FULLY_TRANSPARENT 0
#define EEL_OPACITY_FULLY_OPAQUE      255

extern const EelIRect eel_gdk_pixbuf_whole_pixbuf;

typedef struct EelPixbufLoadHandle EelPixbufLoadHandle;
typedef void (* EelPixbufLoadCallback) (GError         *error,
					GdkPixbuf      *pixbuf,
					gpointer        callback_data);

/* Convenience functions for lists of GdkPixbuf objects. */
void                 eel_gdk_pixbuf_list_ref                  (GList                 *pixbuf_list);
void                 eel_gdk_pixbuf_list_unref                (GList                 *pixbuf_list);
void                 eel_gdk_pixbuf_list_free                 (GList                 *pixbuf_list);


/* Loading a GdkPixbuf with a URI. */
GdkPixbuf *          eel_gdk_pixbuf_load                      (const char            *uri);
GdkPixbuf *          eel_gdk_pixbuf_load_from_stream          (GInputStream          *stream);
GdkPixbuf *          eel_gdk_pixbuf_load_from_stream_at_size  (GInputStream          *stream,
							       int                    size);


/* Same thing async. */
EelPixbufLoadHandle *eel_gdk_pixbuf_load_async                (const char            *uri,
							       int                    priority,
							       EelPixbufLoadCallback  callback,
							       gpointer               callback_data);
void                 eel_cancel_gdk_pixbuf_load               (EelPixbufLoadHandle   *handle);
GdkPixbuf *          eel_gdk_pixbuf_scale_down_to_fit         (GdkPixbuf             *pixbuf,
							       int                    max_width,
							       int                    max_height);
GdkPixbuf *          eel_gdk_pixbuf_scale_to_fit              (GdkPixbuf             *pixbuf,
							       int                    max_width,
							       int                    max_height);
double               eel_gdk_scale_to_fit_factor              (int                    width,
							       int                    height,
							       int                    max_width,
							       int                    max_height,
							       int                   *scaled_width,
							       int                   *scaled_height);
GdkPixbuf *          eel_gdk_pixbuf_scale_to_min              (GdkPixbuf             *pixbuf,
							       int                    min_width,
							       int                    min_height);
double              eel_gdk_scale_to_min_factor               (int                   width,
                                                               int                   height,
							       int                   min_width,
							       int                   min_height,
							       int                   *scaled_width,
							       int                   *scaled_height);

/* return average color values for each component (argb) */
guint32              eel_gdk_pixbuf_average_value             (GdkPixbuf             *pixbuf);
void                 eel_gdk_pixbuf_fill_rectangle_with_color (GdkPixbuf             *pixbuf,
							       EelIRect               area,
							       guint32                color);


/* Save a pixbuf to a png file.  Return value indicates succss/TRUE or failure/FALSE */
gboolean             eel_gdk_pixbuf_save_to_file              (const GdkPixbuf       *pixbuf,
							       const char            *file_name);
void                 eel_gdk_pixbuf_ref_if_not_null           (GdkPixbuf             *pixbuf_or_null);
void                 eel_gdk_pixbuf_unref_if_not_null         (GdkPixbuf             *pixbuf_or_null);


/* Copy a pixbuf to an area of a GdkDrawable */
void                 eel_gdk_pixbuf_draw_to_drawable          (const GdkPixbuf       *pixbuf,
							       GdkDrawable           *drawable,
							       GdkGC                 *gc,
							       int                    source_x,
							       int                    source_y,
							       EelIRect               destination_area,
							       GdkRgbDither           dither,
							       GdkPixbufAlphaMode     alpha_compositing_mode,
							       int                    alpha_threshold);

/* Copy a pixbuf to an area of another pixbuf */
void                 eel_gdk_pixbuf_draw_to_pixbuf            (const GdkPixbuf       *pixbuf,
							       GdkPixbuf             *destination_pixbuf,
							       int                    source_x,
							       int                    source_y,
							       EelIRect               destination_area);


/* Composite one pixbuf over another with the given opacity */
void                 eel_gdk_pixbuf_draw_to_pixbuf_alpha      (const GdkPixbuf       *pixbuf,
							       GdkPixbuf             *destination_pixbuf,
							       int                    source_x,
							       int                    source_y,
							       EelIRect               destination_area,
							       int                    opacity,
							       GdkInterpType          interpolation_mode);


/* Create a pixbuf from a sub area of another pixbuf */
GdkPixbuf *          eel_gdk_pixbuf_new_from_pixbuf_sub_area  (GdkPixbuf             *pixbuf,
							       EelIRect               area);
/* Create a pixbuf from an existing buffer. */
GdkPixbuf *          eel_gdk_pixbuf_new_from_existing_buffer  (guchar                *buffer,
							       int                    buffer_rowstride,
							       gboolean               buffer_has_alpha,
							       EelIRect               area);

/* Determine whether a pixbuf is valid or not */
gboolean             eel_gdk_pixbuf_is_valid                  (const GdkPixbuf       *pixbuf);

/* Access the dimensions of a pixbuf. */
EelDimensions        eel_gdk_pixbuf_get_dimensions            (const GdkPixbuf       *pixbuf);

/* Return the intersection of the pixbuf with the given rectangle. */
EelIRect             eel_gdk_pixbuf_intersect                 (const GdkPixbuf       *pixbuf,
							       int                    pixbuf_x,
							       int                    pixbuf_y,
							       EelIRect               rectangle);

/* Scales large pixbufs down fast */
GdkPixbuf *          eel_gdk_pixbuf_scale_down                (GdkPixbuf *pixbuf,
							       int dest_width,
							       int dest_height);

#endif /* EEL_GDK_PIXBUF_EXTENSIONS_H */
