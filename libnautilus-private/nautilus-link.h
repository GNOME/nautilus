/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.h: xml-based link files that control their appearance
   and behavior.

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
  
   Authors: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_LINK_H
#define NAUTILUS_LINK_H

#include <gdk/gdktypes.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

/* Create a new link file. Takes a path, works locally, and uses sync. I/O.
 * Returns TRUE if it succeeds, FALSE if it fails.
 */
gboolean         nautilus_link_local_create                      (const char       *directory_uri,
								  const char       *file_name,
								  const char       *display_name,
								  const char       *image,
								  const char       *target_uri,
								  const GdkPoint   *point,
								  int               screen,
								  gboolean          unique_filename);

/* Returns additional text to display under the name, NULL if
 * none. Despite the fact that it takes a URI parameter, works only if
 * the file is local and does sync. I/O.
 */
char *           nautilus_link_local_get_additional_text         (const char       *uri);



/* Returns the link uri associated with a link file. The first version
 * works only if the file is local and does sync. I/O, despite the
 * fact that it takes a URI parameter. The second version takes the
 * contents of a file already in memory.
 */
char *           nautilus_link_local_get_link_uri                (const char       *uri);
void           nautilus_link_get_link_info_given_file_contents (const char       *file_contents,
								int               link_file_size,
								char            **uri,
								char            **name,
								char            **icon,
								gulong           *drive_id,
								gulong           *volume_id);
void             nautilus_link_local_create_from_gnome_entry     (GnomeDesktopItem *item,
								  const char       *dest_uri,
								  const GdkPoint   *position,
								  int               screen);

#endif /* NAUTILUS_LINK_H */
