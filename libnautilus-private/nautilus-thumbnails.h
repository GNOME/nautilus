/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-thumbnails.h: Thumbnail code for icon factory.
 
   Copyright (C) 2000 Eazel, Inc.
  
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
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_THUMBNAILS_H
#define NAUTILUS_THUMBNAILS_H

#include "nautilus-file.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Returns NULL if there's no thumbnail yet. */
char *     nautilus_get_thumbnail_uri               (NautilusFile *file);
gboolean   nautilus_thumbnail_has_invalid_thumbnail (NautilusFile *file);
GdkPixbuf *nautilus_thumbnail_load_framed_image     (const char   *path,
						     gboolean      anti_alias_frame_edges);
void       nautilus_update_thumbnail_file_renamed   (const char   *old_file_uri,
						     const char   *new_file_uri);
void       nautilus_remove_thumbnail_for_file       (const char   *old_file_uri);

#endif /* NAUTILUS_THUMBNAILS_H */
