/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-icon-container-grid.h - Grid used by icon container.

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@eazel.com>
*/

#include "gnome-icon-container-private.h"

struct GnomeIconContainerGrid {
	/* Size of the grid. */
	guint width, height;

	/* This is the width that we can actually use for finding an empty
	 * position.
	 */
	guint visible_width;

	/* Array of grid elements. */
	GList **elems;

	/* Size of the allocated array. */
	guint alloc_width, alloc_height;

	/* Position of the first free cell (used to speed up progressive
	 * updates). If negative, there is no free cell.
	 */
	int first_free_x, first_free_y;
};


GnomeIconContainerGrid *gnome_icon_container_grid_new             (void);
void                    gnome_icon_container_grid_destroy         (GnomeIconContainerGrid *grid);
void gnome_icon_container_grid_clear (GnomeIconContainerGrid *grid);
void                    gnome_icon_container_grid_add             (GnomeIconContainerGrid *grid,
								   GnomeIconContainerIcon *icon,
								   guint                   x,
								   guint                   y);
void gnome_icon_container_grid_add_auto (GnomeIconContainerGrid *grid,
		    GnomeIconContainerIcon *icon,
				    guint *x_return, guint *y_return);
void                    gnome_icon_container_grid_remove          (GnomeIconContainerGrid *grid,
								   GnomeIconContainerIcon *icon,
								   guint                   x,
								   guint                   y);
void gnome_icon_container_grid_set_visible_width (GnomeIconContainerGrid *grid,
					     guint visible_width);
void gnome_icon_container_grid_resize (GnomeIconContainerGrid *grid,
				       guint width, guint height);

GList **                gnome_icon_container_grid_get_element_ptr (GnomeIconContainerGrid *grid,
								   guint                   x,
								   guint                   y);
GnomeIconContainerIcon *gnome_icon_container_grid_find_first      (GnomeIconContainer     *container,
								   gboolean                selected_only);
GnomeIconContainerIcon *gnome_icon_container_grid_find_last       (GnomeIconContainer     *container,
								   gboolean                selected_only);
void                    gnome_icon_container_world_to_grid        (GnomeIconContainer     *container,
								   int                     world_x,
								   int                     world_y,
								   guint                  *grid_x,
								   guint                  *grid_y);
void                    gnome_icon_container_grid_to_world        (GnomeIconContainer     *container,
								   guint                   grid_x,
								   guint                   grid_y,
								   int                    *world_x,
								   int                    *world_y);
