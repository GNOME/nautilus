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
*/

#ifndef NAUTILUS_GDK_PIXBUF_EXTENSIONS_H
#define NAUTILUS_GDK_PIXBUF_EXTENSIONS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libart_lgpl/art_rect.h>

typedef struct NautilusPixbufLoadHandle NautilusPixbufLoadHandle;
typedef void (* NautilusPixbufLoadCallback) (GnomeVFSResult  error,
					     GdkPixbuf      *pixbuf,
					     gpointer        callback_data);

/* Convenience functions for lists of GdkPixbuf objects. */
void                      nautilus_gdk_pixbuf_list_ref                 (GList                      *pixbuf_list);
void                      nautilus_gdk_pixbuf_list_unref               (GList                      *pixbuf_list);
void                      nautilus_gdk_pixbuf_list_free                (GList                      *pixbuf_list);


/* Loading a GdkPixbuf with a URI. */
GdkPixbuf *               nautilus_gdk_pixbuf_load                     (const char                 *uri);


/* Same thing async. */
NautilusPixbufLoadHandle *nautilus_gdk_pixbuf_load_async               (const char                 *uri,
									NautilusPixbufLoadCallback  callback,
									gpointer                    callback_data);
void                      nautilus_cancel_gdk_pixbuf_load              (NautilusPixbufLoadHandle   *handle);


/* Draw a GdkPixbuf tiled. */
void                      nautilus_gdk_pixbuf_render_to_drawable_tiled (GdkPixbuf                  *pixbuf,
									GdkDrawable                *drawable,
									GdkGC                      *gc,
									const GdkRectangle         *destination_rectangle,
									GdkRgbDither                dither,
									int                         x_dither,
									int                         y_dither);
GdkPixbuf *               nautilus_gdk_pixbuf_scale_to_fit             (GdkPixbuf                  *pixbuf,
									int                         max_width,
									int                         max_height);

/* return average color values for each component */
void                      nautilus_gdk_pixbuf_average_value            (GdkPixbuf                  *pixbuf,
									GdkColor                   *result_color);

/* Draw text onto a GdkPixbuf using the given font and rect */
void                      nautilus_gdk_pixbuf_draw_text                (GdkPixbuf                  *pixbuf,
									const GdkFont              *font,
									const double		   font_scale,
									const ArtIRect             *destination_rect,
									const char                 *text,
									guint			   overall_alpha);

/* Add white text to a graphics widget */
void			nautilus_gdk_pixbuf_draw_text_white		(GdkPixbuf			*pixbuf,
									 const GdkFont			*font,
									 const ArtIRect			*destination_rect,
									 const char			*text,
									 guint				overall_alpha);

#endif /* NAUTILUS_GDK_PIXBUF_EXTENSIONS_H */
