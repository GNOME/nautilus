/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gmp,e-extensions.h - interface for new functions that operate on
                                 gnome classes. Perhaps some of these should be
  			         rolled into gnome someday.

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

#ifndef NAUTILUS_GNOME_EXTENSIONS_H
#define NAUTILUS_GNOME_EXTENSIONS_H

#include <libgnomeui/gnome-canvas.h>
#include <gtk/gtkwindow.h>

typedef struct {
	int x;
	int y;
} NautilusArtIPoint;

/* This is more handy than gnome_canvas_item_get_bounds because it
 * always returns the bounds * in world coordinates and it returns
 * them in a single rectangle.
 */
void       nautilus_gnome_canvas_item_get_world_bounds          (GnomeCanvasItem *item,
								 ArtDRect        *world_bounds);

/* This returns the current canvas bounds as computed by update.
 * It's not as "up to date" as get_bounds, which is accurate even
 * before an update happens.
 */
void       nautilus_gnome_canvas_item_get_current_canvas_bounds (GnomeCanvasItem *item,
								 ArtIRect        *canvas_bounds);

/* Convenience functions for doing things with whole rectangles. */
void       nautilus_gnome_canvas_world_to_canvas_rectangle      (GnomeCanvas     *canvas,
								 const ArtDRect  *world_rectangle,
								 ArtIRect        *canvas_rectangle);
void       nautilus_gnome_canvas_world_to_window_rectangle      (GnomeCanvas     *canvas,
								 const ArtDRect  *world_rectangle,
								 ArtIRect        *window_rectangle);
void       nautilus_gnome_canvas_request_redraw_rectangle       (GnomeCanvas     *canvas,
								 const ArtIRect  *canvas_rectangle);

/* Requests the entire object be redrawn.
 * Normally, you use request_update when calling from outside the canvas item
 * code. This is for within canvas item code.
 */
void       nautilus_gnome_canvas_item_request_redraw            (GnomeCanvasItem *item);

/* More functions for ArtIRect and ArtDRect. */
gboolean   nautilus_art_irect_equal                             (const ArtIRect  *rect_a,
								 const ArtIRect  *rect_b);
gboolean   nautilus_art_drect_equal                             (const ArtDRect  *rect_a,
								 const ArtDRect  *rect_b);
gboolean   nautilus_art_irect_hits_irect                        (const ArtIRect  *rect_a,
								 const ArtIRect  *rect_b);
gboolean   nautilus_art_irect_contains_irect                    (const ArtIRect  *outer_rect,
								 const ArtIRect  *inner_rect);
int        nautilus_simple_dialog                               (GtkWidget       *parent,
								 const char      *text,
								 const char      *title,
								 ...);

/* Variations on gnome dialogs that use text with line-wrapping turned on. */
GtkWidget *nautilus_error_dialog                                (const char      *error);
GtkWidget *nautilus_error_dialog_parented                       (const char      *error,
								 GtkWindow       *parent);

#endif /* NAUTILUS_GNOME_EXTENSIONS_H */
