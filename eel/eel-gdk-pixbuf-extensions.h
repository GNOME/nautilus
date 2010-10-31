/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-pixbuf-extensions.h: Routines to augment what's in gdk-pixbuf.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
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

#define EEL_OPACITY_FULLY_TRANSPARENT 0
#define EEL_OPACITY_FULLY_OPAQUE      255

/* Convenience functions for lists of GdkPixbuf objects. */
void                 eel_gdk_pixbuf_list_ref                  (GList                 *pixbuf_list);

/* Loading a GdkPixbuf with a URI. */
GdkPixbuf *          eel_gdk_pixbuf_load_from_stream_at_size  (GInputStream          *stream,
							       int                    size);

GdkPixbuf *          eel_gdk_pixbuf_render                    (GdkPixbuf *pixbuf,
                                                               guint render_mode,
                                                               guint saturation,
                                                               guint brightness,
                                                               guint lighten_value,
                                                               guint color);

#endif /* EEL_GDK_PIXBUF_EXTENSIONS_H */
