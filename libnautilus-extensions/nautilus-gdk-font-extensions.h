/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gdk-extensions.h: GdkFont extensions.

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
            Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_GDK_FONT_EXTENSIONS_H
#define NAUTILUS_GDK_FONT_EXTENSIONS_H

#include <gdk/gdk.h>

/* Misc GdkFont helper functions */
gboolean nautilus_gdk_font_equal               (GdkFont    *font_a_null_allowed,
						GdkFont    *font_b_null_allowed);
GdkFont *nautilus_gdk_font_get_largest_fitting (GdkFont    *font,
						const char *text,
						int         available_width,
						int         minimum_acceptable_font_size,
						int         maximum_acceptable_font_size);
GdkFont *nautilus_gdk_font_get_bold            (GdkFont    *plain);
GdkFont *nautilus_gdk_font_get_larger          (GdkFont    *font,
						int         num_sizes);
GdkFont *nautilus_gdk_font_get_smaller         (GdkFont    *font,
						int         num_sizes);
GdkFont *nautilus_gdk_font_get_fixed           (void);
char *   nautilus_string_ellipsize_start       (const char *original,
						GdkFont    *font,
						int         length);
char *   nautilus_gdk_font_xlfd_string_new     (const char *foundry,
						const char *family,
						const char *weight,
						const char *slant,
						const char *set_width,
						const char *add_style,
						guint       size_in_pixels);

#endif /* NAUTILUS_GDK_FONT_EXTENSIONS_H */
