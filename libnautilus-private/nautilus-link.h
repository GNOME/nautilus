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

#include "nautilus-file.h"
#include <gdk/gdk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-dentry.h>

/* Link types */
typedef enum {
	NAUTILUS_LINK_GENERIC,
	NAUTILUS_LINK_TRASH,
	NAUTILUS_LINK_MOUNT,
	NAUTILUS_LINK_HOME
} NautilusLinkType;

/* Create a new link file. Takes a path, works locally, and uses sync. I/O.
 * Returns TRUE if it succeeds, FALSE if it fails.
 */
gboolean         nautilus_link_local_create                     (const char       *directory_path,
								 const char       *name,
								 const char       *image,
								 const char       *target_uri,
								 const GdkPoint   *point,
								 NautilusLinkType  type);

/* Change the icon of an existing link file. Takes a path, works
 * locally, and uses sync. I/O. Returns TRUE if it succeeds, FALSE if
 * it fails. Does not check and see if it is a link file.
 */
gboolean         nautilus_link_local_set_icon                   (const char       *path,
								 const char       *icon_name);

/* Specify the type of link that is represented
 * Takes a path, works locally, and uses sync. I/O. 
 * Returns TRUE if it succeeds, FALSE if
 * it fails. Does not check and see if it is a link file.
 */
gboolean         nautilus_link_local_set_type                   (const char       *path,
								 NautilusLinkType  type);

/* Specify the link uri of link that is represented
 * Takes a path, works locally, and uses sync. I/O. 
 * Returns TRUE if it succeeds, FALSE if
 * it fails. Does not check and see if it is a link file.
 */
gboolean         nautilus_link_local_set_link_uri               (const char       *path,
								 const char       *uri);

/* Returns additional text to display under the name, NULL if
 * none. Despite the fact that it takes a URI parameter, works only if
 * the file is local and does sync. I/O.
 */
char *           nautilus_link_local_get_additional_text        (const char       *path);

/* Returns the link type of a link file.
 * Works only if the file is local and does sync. I/O
 */
NautilusLinkType nautilus_link_local_get_link_type              (const char       *path);


/* Returns if a link is a mount link.
 * Works only if the file is local and does sync. I/O
 */
gboolean         nautilus_link_local_is_volume_link             (const char       *path);

/* Returns if a link is a home link.
 * Works only if the file is local and does sync. I/O
 */
gboolean         nautilus_link_local_is_home_link               (const char       *path);

/* Returns if a link is a trash link.
 * Works only if the file is local and does sync. I/O
 */
gboolean         nautilus_link_local_is_trash_link              (const char       *path);


/* Returns the link uri associated with a link file. The first version
 * works only if the file is local and does sync. I/O, despite the
 * fact that it takes a URI parameter. The second version takes the
 * contents of a file already in memory.
 */
char *           nautilus_link_local_get_link_uri               (const char       *path);
char *           nautilus_link_get_link_uri_given_file_contents (const char       *link_file_contents,
								 int               link_file_size);
char *           nautilus_link_get_link_name_given_file_contents (const char      *file_contents,
								  int              link_file_size);
char *           nautilus_link_get_link_icon_given_file_contents (const char      *file_contents,
								  int              link_file_size);
void		 nautilus_link_local_create_from_gnome_entry 	 (GnomeDesktopEntry *entry,
								  const char 	   *dest_path,
								  const GdkPoint    *position);
								 								 
#endif /* NAUTILUS_LINK_H */
