/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-find-icon-image.h: Functions that locate icon image files,
                               used internally by the icon factory.
 
   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 1999, 2000 Eazel, Inc.
   Copyright (C) 2001 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_FIND_ICON_IMAGE_H
#define NAUTILUS_FIND_ICON_IMAGE_H

#include <libnautilus-private/nautilus-icon-factory.h>
#include <libart_lgpl/art_rect.h>

#define NAUTILUS_EMBLEM_NAME_PREFIX "emblem-"

typedef struct {
	char *name;
	gboolean is_in_user_directory;
} NautilusIconTheme;

typedef struct {
	NautilusIconTheme current;
	NautilusIconTheme fallback;
} NautilusIconThemeSpecifications;

typedef struct {
	ArtIRect text_rect;
	NautilusEmblemAttachPoints attach_points;
} NautilusIconDetails;

char *nautilus_get_icon_file_name           (const NautilusIconThemeSpecifications *theme,
					     const char                            *name,
					     const char                            *modifier,
					     guint                                  size_in_pixels,
					     gboolean                               optimized_for_aa,
					     NautilusIconDetails                   *details);
char *nautilus_remove_icon_file_name_suffix (const char                            *name);

#endif
