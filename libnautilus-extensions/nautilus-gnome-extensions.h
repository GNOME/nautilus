/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-gnome-extensions.h - interface for new functions that operate on
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
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkwindow.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Causes an update as needed. The GnomeCanvas code says it should, but it doesn't. */
void       nautilus_gnome_canvas_set_scroll_region                      (GnomeCanvas     *canvas,
									 double           x1,
									 double           y1,
									 double           x2,
									 double           y2);

/* Make the scroll region bigger so the code in GnomeCanvas won't center it. */
void       nautilus_gnome_canvas_set_scroll_region_left_justify         (GnomeCanvas     *canvas,
									 double           x1,
									 double           y1,
									 double           x2,
									 double           y2);

/* Set a new scroll region without eliminating any of the currently-visible area. */
void       nautilus_gnome_canvas_set_scroll_region_include_visible_area (GnomeCanvas     *canvas,
									 double           x1,
									 double           y1,
									 double           x2,
									 double           y2);

/* For cases where you need to get more than one item updated. */
void       nautilus_gnome_canvas_request_update_all                     (GnomeCanvas     *canvas);
void       nautilus_gnome_canvas_item_request_update_deep               (GnomeCanvasItem *item);

/* This is more handy than gnome_canvas_item_get_bounds because it
 * always returns the bounds * in world coordinates and it returns
 * them in a single rectangle.
 */
void       nautilus_gnome_canvas_item_get_world_bounds                  (GnomeCanvasItem *item,
									 ArtDRect        *world_bounds);

/* This returns the current canvas bounds as computed by update.
 * It's not as "up to date" as get_bounds, which is accurate even
 * before an update happens.
 */
void       nautilus_gnome_canvas_item_get_current_canvas_bounds         (GnomeCanvasItem *item,
									 ArtIRect        *canvas_bounds);

/* This returns the canvas bounds in a slower way that's always up to
 * date, even before an update.
 */
void       nautilus_gnome_canvas_item_get_canvas_bounds                 (GnomeCanvasItem *item,
									 ArtIRect        *canvas_bounds);

/* Convenience functions for doing things with whole rectangles. */
void       nautilus_gnome_canvas_world_to_canvas_rectangle              (GnomeCanvas     *canvas,
									 const ArtDRect  *world_rectangle,
									 ArtIRect        *canvas_rectangle);
void       nautilus_gnome_canvas_world_to_window_rectangle              (GnomeCanvas     *canvas,
									 const ArtDRect  *world_rectangle,
									 ArtIRect        *window_rectangle);
void       nautilus_gnome_canvas_request_redraw_rectangle               (GnomeCanvas     *canvas,
									 const ArtIRect  *canvas_rectangle);

/* Requests the entire object be redrawn.
 * Normally, you use request_update when calling from outside the canvas item
 * code. This is for within canvas item code.
 */
void       nautilus_gnome_canvas_item_request_redraw                    (GnomeCanvasItem *item);
void       nautilus_gnome_canvas_draw_pixbuf                            (GnomeCanvasBuf  *buf,
									 const GdkPixbuf *pixbuf,
									 int              x,
									 int              y);
void       nautilus_gnome_canvas_fill_rgb                               (GnomeCanvasBuf  *buf,
									 art_u8           r,
									 art_u8           g,
									 art_u8           b);

/* More functions for GnomeDialog */
GtkButton *nautilus_gnome_dialog_get_button_by_index                    (GnomeDialog     *dialog,
									 int              index);

/* Return a command string containing the path to a terminal on this system. */
char 	  *nautilus_gnome_get_terminal_path 				(void);

/* Open up a new terminal, optionally passing in a command to execute */
void       nautilus_gnome_open_terminal                                 (const char      *command);

/* Set an icon on GnomeStock widget. If the setting fails register this
 * as a new file.  Returns FALSE if even that failed.
 */
gboolean   nautilus_gnome_stock_set_icon_or_register                    (GnomeStock      *stock,
									 const char      *icon);

/* Execute a command. To be used in place of the exec family commands. This function
 * protects against creating zombie processes.
*/
int 	   nautilus_gnome_shell_execute 				 (const char 	 *command);

#endif /* NAUTILUS_GNOME_EXTENSIONS_H */
