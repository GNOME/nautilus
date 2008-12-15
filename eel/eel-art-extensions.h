/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-extensions.h - interface of libart extension functions.

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

#ifndef EEL_ART_EXTENSIONS_H
#define EEL_ART_EXTENSIONS_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
  double x, y;
} EelDPoint;

typedef struct {
	int x;
	int y;
} EelIPoint;

typedef struct  {
  double x0, y0, x1, y1;
} EelDRect;

typedef struct  {
  /*< public >*/
  int x0, y0, x1, y1;
} EelIRect;

typedef struct {
	int width;
	int height;
} EelDimensions;

extern const EelDRect eel_drect_empty;
extern const EelIRect eel_irect_empty;
extern const EelIPoint eel_ipoint_max;
extern const EelIPoint eel_ipoint_min;
extern const EelIPoint eel_ipoint_zero;
extern const EelDimensions eel_dimensions_empty;

void     eel_irect_copy              (EelIRect       *dest,
				      const EelIRect *src);
void     eel_irect_union             (EelIRect       *dest,
				      const EelIRect *src1,
				      const EelIRect *src2);
void     eel_irect_intersect         (EelIRect       *dest,
				      const EelIRect *src1,
				      const EelIRect *src2);
gboolean eel_irect_equal             (EelIRect        rectangle_a,
				      EelIRect        rectangle_b);
gboolean eel_irect_hits_irect        (EelIRect        rectangle_a,
				      EelIRect        rectangle_b);
EelIRect eel_irect_offset_by         (EelIRect        rectangle,
				      int             x,
				      int             y);
EelIRect eel_irect_scale_by          (EelIRect        rectangle,
				      double          scale);
gboolean eel_irect_is_empty          (const EelIRect *rectangle);
gboolean eel_irect_contains_point    (EelIRect        outer_rectangle,
				      int             x,
				      int             y);
EelIRect eel_irect_assign            (int             x,
				      int             y,
				      int             width,
				      int             height);
EelIRect eel_irect_assign_dimensions (int             x,
				      int             y,
				      EelDimensions   dimensions);
int      eel_irect_get_width         (EelIRect        rectangle);
int      eel_irect_get_height        (EelIRect        rectangle);
EelIRect eel_irect_align             (EelIRect        container,
				      int             aligned_width,
				      int             aligned_height,
				      float           x_alignment,
				      float           y_alignment);


void eel_drect_union (EelDRect       *dest,
		      const EelDRect *src1,
		      const EelDRect *src2);


/* EelDimensions functions. */
gboolean      eel_dimensions_are_empty        (EelDimensions dimensions);


G_END_DECLS

#endif /* EEL_ART_EXTENSIONS_H */
