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
gboolean nautilus_gdk_font_equal           (GdkFont       *font_a_null_allowed,
					    GdkFont       *font_b_null_allowed);
GdkFont *nautilus_get_largest_fitting_font (const char    *text_to_format,
					    int            width,
					    const char    *font_format);
GdkFont *nautilus_gdk_font_get_bold        (const GdkFont *plain);
GdkFont *nautilus_gdk_font_get_larger      (const GdkFont *font,
					    int            num_sizes);
GdkFont *nautilus_gdk_font_get_smaller     (const GdkFont *font,
					    int            num_sizes);
char *   nautilus_string_ellipsize_start   (const char    *original,
					    GdkFont       *font,
					    int            length);

#endif /* NAUTILUS_GDK_FONT_EXTENSIONS_H */
