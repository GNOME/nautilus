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

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@eazel.com>
*/

#include "gnome-icon-container-private.h"

/* setting up the grid */
GnomeIconContainerGrid *gnome_icon_container_grid_new                    (void);
void                    gnome_icon_container_grid_destroy                (GnomeIconContainerGrid                *grid);
void                    gnome_icon_container_grid_clear                  (GnomeIconContainerGrid                *grid);
void                    gnome_icon_container_grid_set_visible_width      (GnomeIconContainerGrid                *grid,
									  double                                 world_visible_width);

/* getting icons in and out of the grid */
void                    gnome_icon_container_grid_add                    (GnomeIconContainerGrid                *grid,
									  GnomeIconContainerIcon                *icon);
void                    gnome_icon_container_grid_remove                 (GnomeIconContainerGrid                *grid,
									  GnomeIconContainerIcon                *icon);
void                    gnome_icon_container_grid_get_position           (GnomeIconContainerGrid                *grid,
									  GnomeIconContainerIcon                *icon,
									  ArtPoint                              *world_point);

/* getting groups of icons in parts of the grid */
GList                  *gnome_icon_container_grid_get_intersecting_icons (GnomeIconContainerGrid                *grid,
									  const ArtDRect                        *world_rectangle);
