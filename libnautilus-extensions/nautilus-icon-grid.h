/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-grid.h - Grid used by icon container.

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

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@eazel.com>
*/

#include "nautilus-icon-private.h"

/* setting up the grid */
NautilusIconGrid *      nautilus_icon_grid_new                    (void);
void                    nautilus_icon_grid_destroy                (NautilusIconGrid *grid);
void                    nautilus_icon_grid_clear                  (NautilusIconGrid *grid);
void                    nautilus_icon_grid_set_visible_width      (NautilusIconGrid *grid,
								   double            world_visible_width);

/* getting icons in and out of the grid */
void                    nautilus_icon_grid_add                    (NautilusIconGrid *grid,
								   NautilusIcon     *icon);
void                    nautilus_icon_grid_remove                 (NautilusIconGrid *grid,
								   NautilusIcon     *icon);
void                    nautilus_icon_grid_get_position           (NautilusIconGrid *grid,
								   NautilusIcon     *icon,
								   ArtPoint         *world_point);

/* getting groups of icons in parts of the grid */
GList                  *nautilus_icon_grid_get_intersecting_icons (NautilusIconGrid *grid,
								   const ArtDRect   *world_rectangle);
