/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities.h - interface for file manipulation routines.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_FILE_UTILITIES_H
#define NAUTILUS_FILE_UTILITIES_H

#include <libgnomevfs/gnome-vfs-types.h>

typedef void (* NautilusReadFileCallback) (GnomeVFSResult result,
					   GnomeVFSFileSize file_size,
					   char *file_contents,
					   gpointer callback_data);
typedef struct NautilusReadFileHandle NautilusReadFileHandle;

char *                  nautilus_format_uri_for_display  (const char                *uri);
char *                  nautilus_make_uri_from_input     (const char                *location);
char *                  nautilus_make_path               (const char                *path,
							  const char                *name);

/* Return paths that don't need to be destroyed. We will probably
 * change these to return ones that do need to be destroyed for
 * consistency soon.
 */
const char *            nautilus_get_user_directory      (void);
const char *            nautilus_get_user_main_directory (void);
const char *            nautilus_get_desktop_directory   (void);
const char *            nautilus_get_pixmap_directory    (void);

/* Turn a "file://" URI into a local path.
 * Returns NULL if it's not a URI that can be converted.
 */
char *                  nautilus_get_local_path_from_uri (const char                *uri);

/* Turn a path into a "file://" URI. */
char *                  nautilus_get_uri_from_local_path (const char                *local_full_path);

/* A version of gnome's gnome_pixmap_file that works for the nautilus prefix.
 * Otherwise similar to gnome_pixmap_file in that it checks to see if the file
 * exists and returns NULL if it doesn't.
 */
char *                  nautilus_pixmap_file             (const char                *partial_path);

/* Read an entire file at once with gnome-vfs. */
GnomeVFSResult          nautilus_read_entire_file        (const char                *uri,
							  int                       *file_size,
							  char                     **file_contents);
NautilusReadFileHandle *nautilus_read_entire_file_async  (const char                *uri,
							  NautilusReadFileCallback   calllback,
							  gpointer                   callback_data);
void                    nautilus_read_entire_file_cancel (NautilusReadFileHandle    *handle);

#endif /* NAUTILUS_FILE_UTILITIES_H */
