/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   gdk-extensions.h: Graphics routines to augment what's in gdk.

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

/* A gradient spec. is a string that contains a specifier for either a
   color or a gradient. If the string has a "-" in it, then it's a gradient.
   The gradient is vertical by default and the spec. can end with ":v" to indicate that.
   If the gradient ends with ":h", the gradient is horizontal.
*/
char *   nautilus_gradient_new                       (const char         *start_color,
						      const char         *end_color,
						      gboolean            is_horizontal);

gboolean nautilus_gradient_is_gradient               (const char         *gradient_spec);
char *   nautilus_gradient_get_start_color_spec      (const char         *gradient_spec);
char *   nautilus_gradient_get_end_color_spec        (const char         *gradient_spec);
gboolean nautilus_gradient_is_horizontal             (const char         *gradient_spec);

char *   nautilus_gradient_set_left_color_spec       (const char         *gradient_spec,
						      const char         *left_color);
char *   nautilus_gradient_set_top_color_spec        (const char         *gradient_spec,
						      const char         *top_color);
char *   nautilus_gradient_set_right_color_spec      (const char         *gradient_spec,
						      const char         *right_color);
char *   nautilus_gradient_set_bottom_color_spec     (const char         *gradient_spec,
						      const char         *bottom_color);

/* A version of parse_color that substitutes a default color instead of returning
   a boolean to indicate it cannot be parsed.
*/
void     nautilus_gdk_color_parse_with_default       (const char         *color_spec,
						      const GdkColor     *default_color,
						      GdkColor           *color);
void     nautilus_gdk_color_parse_with_white_default (const char         *color_spec,
						      GdkColor           *color);

/* Fill routines that take GdkRectangle parameters instead of four integers. */
void     nautilus_fill_rectangle                     (GdkDrawable        *drawable,
						      GdkGC              *gc,
						      const GdkRectangle *rectangle);
void     nautilus_fill_rectangle_with_color          (GdkDrawable        *drawable,
						      GdkGC              *gc,
						      const GdkRectangle *rectangle,
						      const GdkColor     *color);
void     nautilus_fill_rectangle_with_gradient       (GdkDrawable        *drawable,
						      GdkGC              *gc,
						      GdkColormap        *colormap,
						      const GdkRectangle *rectangle,
						      const GdkColor     *start_color,
						      const GdkColor     *end_color,
						      gboolean            horizontal_gradient);

/* A basic operation we use for drawing gradients is interpolating two colors.*/
void     nautilus_interpolate_color                  (gdouble             ratio,
						      const GdkColor     *start_color,
						      const GdkColor     *end_color,
						      GdkColor           *interpolated_color);

#endif /* GDK_EXTENSIONS_H */
