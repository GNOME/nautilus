/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gdk-extensions.h - Possible additions for gdk.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef GDK_EXTENSIONS_H
#define GDK_EXTENSIONS_H

#include <gdk/gdk.h>

/* Maybe these will get new names that don't start with gdk, but for now,
 * I'll name them as if they were part of it.
 */

/* Perhaps we need a separate call that allocates colors for the gradient? */

void gdk_fill_rectangle               (GdkDrawable        *drawable,
				       GdkGC              *gc,
				       const GdkRectangle *rectangle);
void gdk_fill_rectangle_with_color    (GdkDrawable        *drawable,
				       GdkGC              *gc,
				       const GdkRectangle *rectangle,
				       const GdkColor     *color);
void gdk_fill_rectangle_with_gradient (GdkDrawable        *drawable,
				       GdkGC              *gc,
				       GdkColormap        *colormap,
				       const GdkRectangle *rectangle,
				       const GdkColor     *start_color,
				       const GdkColor     *end_color,
				       gboolean            horizontal_gradient);
void gdk_interpolate_color            (gdouble             ratio,
				       const GdkColor     *start_color,
				       const GdkColor     *end_color,
				       GdkColor           *interpolated_color);


#endif /* GDK_EXTENSIONS_H */
