/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-art-extensions.h - interface of libart extension functions.

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
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_ART_EXTENSIONS_H
#define NAUTILUS_ART_EXTENSIONS_H

#include <libgnome/gnome-defs.h>
#include <libart_lgpl/art_rect.h>
#include <glib.h>

BEGIN_GNOME_DECLS

typedef struct {
	int x;
	int y;
} NautilusArtIPoint;

typedef struct {
	int width;
	int height;
} NautilusDimensions;

extern ArtIRect NAUTILUS_ART_IRECT_EMPTY;
extern NautilusArtIPoint NAUTILUS_ART_IPOINT_ZERO;
extern NautilusDimensions NAUTILUS_DIMENSIONS_EMPTY;

/* More functions for ArtIRect and ArtDRect. */
gboolean nautilus_art_irect_equal             (const ArtIRect           *rect_a,
					       const ArtIRect           *rect_b);
gboolean nautilus_art_drect_equal             (const ArtDRect           *rect_a,
					       const ArtDRect           *rect_b);
gboolean nautilus_art_irect_hits_irect        (const ArtIRect           *rect_a,
					       const ArtIRect           *rect_b);
gboolean nautilus_art_irect_contains_irect    (const ArtIRect           *outer_rect,
					       const ArtIRect           *inner_rect);
gboolean nautilus_art_irect_contains_point    (const ArtIRect           *outer_rect,
					       int                       x,
					       int                       y);
gboolean nautilus_art_irect_is_valid          (const ArtIRect           *rect);
void     nautilus_art_irect_assign            (ArtIRect                 *rect,
					       int                       x,
					       int                       y,
					       int                       width,
					       int                       height);
int      nautilus_art_irect_get_width         (const ArtIRect           *rect);
int      nautilus_art_irect_get_height        (const ArtIRect           *rect);
ArtIRect nautilus_art_irect_align             (const ArtIRect           *container,
					       int                       aligned_width,
					       int                       aligned_height,
					       float                     x_alignment,
					       float                     y_alignment);
/* NautilusDimensions functions. */
gboolean nautilus_dimensions_empty            (const NautilusDimensions *dimensions);
ArtIRect nautilus_art_irect_assign_dimensions (int                       x,
					       int                       y,
					       const NautilusDimensions *dimensions);

ArtIRect nautilus_art_irect_offset_by	      (ArtIRect			 rect,
					       int			 x,
					       
					       int			 y);
ArtIRect nautilus_art_irect_offset_to	      (ArtIRect			 rect,
					       int			 x,
					       int			 y);
ArtIRect nautilus_art_irect_scale_by	      (ArtIRect			 rect,
					       double			 scale);
ArtIRect nautilus_art_irect_inset	      (ArtIRect			 rect,
					       int			 horizontal_inset,
					       int			 vertical_inset);
ArtDRect nautilus_art_drect_offset_by	      (ArtDRect			 rect,
					       double			 x,
					       double			 y);
ArtDRect nautilus_art_drect_offset_to	      (ArtDRect			 rect,
					       double			 x,
					       double			 y);
ArtDRect nautilus_art_drect_scale_by	      (ArtDRect			 rect,
					       double			 scale);
ArtDRect nautilus_art_drect_inset	      (ArtDRect			 rect,
					       double			 horizontal_inset,
					       double			 vertical_inset);
ArtIRect nautilus_art_irect_offset_by_point   (ArtIRect			 rect,
					       NautilusArtIPoint	 point);
ArtIRect nautilus_art_irect_offset_to_point   (ArtIRect			 rect,
					       NautilusArtIPoint	 point);
					      
END_GNOME_DECLS

#endif /* NAUTILUS_ART_EXTENSIONS_H */
