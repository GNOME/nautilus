/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-debug-drawing.h: Nautilus drawing debugging aids.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_DEBUG_DRAWING_H
#define NAUTILUS_DEBUG_DRAWING_H

#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>

/* Draw a rectangle and cross on the given window */
void nautilus_debug_draw_rectangle_and_cross    (GdkDrawable     *drawable,
						 const ArtIRect  *rectangle,
						 guint32          color,
						 gboolean         draw_cross);

/* Show the given pixbuf in eog */
void nautilus_debug_show_pixbuf_in_eog          (const GdkPixbuf *pixbuf);

/* Show the given pixbuf in an in process window */
void nautilus_debug_show_pixbuf                 (const GdkPixbuf *pixbuf);

/* Draw a point in a pixbuf */
void nautilus_debug_pixbuf_draw_point           (GdkPixbuf       *pixbuf,
						 int              x,
						 int              y,
						 guint32          color,
						 int              opacity);
/* Draw a rectangle in a pixbuf.  The coordinates (-1,-1( (-1,-1) will use
 * the whole pixbuf. */
void nautilus_debug_pixbuf_draw_rectangle       (GdkPixbuf       *pixbuf,
						 gboolean         filled,
						 int              x0,
						 int              y0,
						 int              x1,
						 int              y1,
						 guint32          color,
						 int              opacity);
/* Draw an inset rectangle in a pixbuf.  Positive inset make the rectangle
 * smaller.  Negative inset makes it larger.
 */
void nautilus_debug_pixbuf_draw_rectangle_inset (GdkPixbuf       *pixbuf,
						 gboolean         filled,
						 int              x0,
						 int              y0,
						 int              x1,
						 int              y1,
						 guint32          color,
						 int              opacity,
						 int              inset);

#endif /* NAUTILUS_DEBUG_DRAWING_H */
