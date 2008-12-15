/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-extensions.h: Graphics routines to augment what's in gdk.

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

   Authors: Darin Adler <darin@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_GDK_EXTENSIONS_H
#define EEL_GDK_EXTENSIONS_H

#include <gdk/gdk.h>

#define EEL_RGB_COLOR_RED	0xFF0000
#define EEL_RGB_COLOR_GREEN	0x00FF00
#define EEL_RGB_COLOR_BLUE	0x0000FF
#define EEL_RGB_COLOR_WHITE	0xFFFFFF
#define EEL_RGB_COLOR_BLACK	0x000000

#define EEL_RGBA_COLOR_OPAQUE_RED	0xFFFF0000
#define EEL_RGBA_COLOR_OPAQUE_GREEN	0xFF00FF00
#define EEL_RGBA_COLOR_OPAQUE_BLUE	0xFF0000FF
#define EEL_RGBA_COLOR_OPAQUE_WHITE	0xFFFFFFFF
#define EEL_RGBA_COLOR_OPAQUE_BLACK	0xFF000000

/* Pack RGBA values into 32 bits */
#define EEL_RGBA_COLOR_PACK(r, g, b, a)		\
( (((guint32)a) << 24) |			\
  (((guint32)r) << 16) |			\
  (((guint32)g) <<  8) |			\
  (((guint32)b) <<  0) )

/* Pack opaque RGBA values into 32 bits */
#define EEL_RGB_COLOR_PACK(r, g, b)		\
EEL_RGBA_COLOR_PACK((r), (g), (b), 0xFF)

/* Access the individual RGBA components */
#define EEL_RGBA_COLOR_GET_R(color) (((color) >> 16) & 0xff)
#define EEL_RGBA_COLOR_GET_G(color) (((color) >> 8) & 0xff)
#define EEL_RGBA_COLOR_GET_B(color) (((color) >> 0) & 0xff)
#define EEL_RGBA_COLOR_GET_A(color) (((color) >> 24) & 0xff)

/* Bits returned by eel_gdk_parse_geometry */
typedef enum {
	EEL_GDK_NO_VALUE     = 0x00,
	EEL_GDK_X_VALUE      = 0x01,
	EEL_GDK_Y_VALUE      = 0x02,
	EEL_GDK_WIDTH_VALUE  = 0x04,
	EEL_GDK_HEIGHT_VALUE = 0x08,
	EEL_GDK_ALL_VALUES   = 0x0f,
	EEL_GDK_X_NEGATIVE   = 0x10,
	EEL_GDK_Y_NEGATIVE   = 0x20
} EelGdkGeometryFlags;

/* A gradient spec. is a string that contains a specifier for either a
   color or a gradient. If the string has a "-" in it, then it's a gradient.
   The gradient is vertical by default and the spec. can end with ":v" to indicate that.
   If the gradient ends with ":h", the gradient is horizontal.
*/
char *              eel_gradient_new                       (const char          *start_color,
							    const char          *end_color,
							    gboolean             is_horizontal);
char *              eel_gradient_parse_one_color_spec      (const char          *spec,
							    int                 *percent,
							    const char         **next_spec);
gboolean            eel_gradient_is_gradient               (const char          *gradient_spec);
char *              eel_gradient_get_start_color_spec      (const char          *gradient_spec);
char *              eel_gradient_get_end_color_spec        (const char          *gradient_spec);
gboolean            eel_gradient_is_horizontal             (const char          *gradient_spec);
char *              eel_gradient_set_left_color_spec       (const char          *gradient_spec,
							    const char          *left_color);
char *              eel_gradient_set_top_color_spec        (const char          *gradient_spec,
							    const char          *top_color);
char *              eel_gradient_set_right_color_spec      (const char          *gradient_spec,
							    const char          *right_color);
char *              eel_gradient_set_bottom_color_spec     (const char          *gradient_spec,
							    const char          *bottom_color);


/* A version of parse_color that substitutes a default color instead of returning
   a boolean to indicate it cannot be parsed.
*/
void                eel_gdk_color_parse_with_default       (const char          *color_spec,
							    const GdkColor      *default_color,
							    GdkColor            *parsed_color);
void                eel_gdk_color_parse_with_white_default (const char          *color_spec,
							    GdkColor            *parsed_color);
guint32             eel_parse_rgb_with_default             (const char          *color_spec,
							    guint32              default_rgb);
guint32             eel_parse_rgb_with_white_default       (const char          *color_spec);
guint32             eel_rgb_shift_color                    (guint32              color,
							    float                shift_by);
guint32             eel_rgb16_to_rgb                       (gushort              r,
							    gushort              g,
							    gushort              b);
guint32             eel_rgb8_to_rgb                        (guchar               r,
							    guchar               g,
							    guchar               b);
guint32             eel_gdk_color_to_rgb                   (const GdkColor      *color);
GdkColor            eel_gdk_rgb_to_color                   (guint32              color);
char *              eel_gdk_rgb_to_color_spec              (guint32              color);

gboolean            eel_gdk_color_is_dark                  (GdkColor            *color);

/* A routine to get a 50% gray stippled bitmap for use in some types of highlighting. */
GdkBitmap *         eel_stipple_bitmap_for_screen          (GdkScreen *screen);
GdkBitmap *         eel_stipple_bitmap                     (void);


/* Misc GdkRectangle helper functions */
gboolean            eel_gdk_rectangle_contains_rectangle   (GdkRectangle         outer,
							    GdkRectangle         inner);


/* A basic operation we use for drawing gradients is interpolating two colors.*/
guint32             eel_interpolate_color                  (gdouble              ratio,
							    guint32              start_rgb,
							    guint32              end_rgb);

/* Misc GdkWindow helper functions */
void                eel_gdk_window_bring_to_front          (GdkWindow           *window);
void                eel_gdk_window_set_invisible_cursor    (GdkWindow           *window);
void                eel_gdk_window_focus                   (GdkWindow           *window,
							    guint32              timestamp);
void                eel_gdk_window_set_wm_protocols        (GdkWindow           *window,
							    GdkAtom             *protocols,
							    int                  nprotocols);


void                eel_gdk_window_set_wm_hints_input      (GdkWindow           *w,
							    gboolean             status);

/* Wrapper for XParseGeometry */
EelGdkGeometryFlags eel_gdk_parse_geometry                 (const char          *string,
							    int                 *x_return,
							    int                 *y_return,
							    guint               *width_return,
							    guint               *height_return);
void                eel_gdk_draw_layout_with_drop_shadow   (GdkDrawable         *drawable,
							    GdkGC               *gc,
							    GdkColor            *text_color,
							    GdkColor            *shadow_color,
							    int                  x,
							    int                  y,
							    PangoLayout         *layout);
#endif /* EEL_GDK_EXTENSIONS_H */
