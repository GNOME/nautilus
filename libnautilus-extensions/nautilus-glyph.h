/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-glyph.h - A wrapper for rsvg glyphs.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_GLYPH_H
#define NAUTILUS_GLYPH_H

#include <libgnome/gnome-defs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_rect.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-art-extensions.h>

BEGIN_GNOME_DECLS

typedef struct NautilusGlyph NautilusGlyph;

NautilusGlyph *    nautilus_glyph_new            (const NautilusScalableFont *font,
						  int                         font_size,
						  const char                 *text,
						  int                         text_length);
void               nautilus_glyph_free           (NautilusGlyph              *glyph);
int                nautilus_glyph_get_width      (const NautilusGlyph        *glyph);
int                nautilus_glyph_get_height     (const NautilusGlyph        *glyph);
NautilusDimensions nautilus_glyph_get_dimensions (const NautilusGlyph        *glyph);
void               nautilus_glyph_draw_to_pixbuf (const NautilusGlyph        *glyph,
						  GdkPixbuf                  *pixbuf,
						  int                         destination_x,
						  int                         destination_y,
						  const ArtIRect             *clip_area,
						  guint32                     color,
						  int                         opacity);
ArtIRect           nautilus_glyph_intersect      (const NautilusGlyph        *glyph,
						  int                         glyph_x,
						  int                         glyph_y,
						  const ArtIRect             *rectangle);

END_GNOME_DECLS

#endif /* NAUTILUS_GLYPH_H */


