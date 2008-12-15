/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-debug-drawing.h: Eel drawing debugging aids.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_DEBUG_DRAWING_H
#define EEL_DEBUG_DRAWING_H

#include <eel/eel-gdk-pixbuf-extensions.h>

/* Draw a rectangle and cross on the given window */
void eel_debug_draw_rectangle_and_cross       (GdkDrawable     *drawable,
					       EelIRect         rectangle,
					       guint32          color,
					       gboolean         draw_cross);

/* Show the given pixbuf in an external out of process viewer */
void eel_debug_show_pixbuf_in_external_viewer (const GdkPixbuf *pixbuf,
					       const char      *viewer_name);

/* Show the given pixbuf in an in process window */
void eel_debug_show_pixbuf                    (GdkPixbuf *pixbuf);

/* Draw a point in a pixbuf */
void eel_debug_pixbuf_draw_point              (GdkPixbuf       *pixbuf,
					       int              x,
					       int              y,
					       guint32          color,
					       int              opacity);
/* Draw a rectangle in a pixbuf.  The coordinates (-1,-1( (-1,-1) will use
 * the whole pixbuf. */
void eel_debug_pixbuf_draw_rectangle          (GdkPixbuf       *pixbuf,
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
void eel_debug_pixbuf_draw_rectangle_inset    (GdkPixbuf       *pixbuf,
					       gboolean         filled,
					       int              x0,
					       int              y0,
					       int              x1,
					       int              y1,
					       guint32          color,
					       int              opacity,
					       int              inset);

#endif /* EEL_DEBUG_DRAWING_H */
