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

#define	NAUTILUS_TRASH_URI	"trash:"

typedef void     (* NautilusReadFileCallback) (GnomeVFSResult result,
					       GnomeVFSFileSize file_size,
					       char *file_contents,
					       gpointer callback_data);
typedef gboolean (* NautilusReadMoreCallback) (GnomeVFSFileSize file_size,
					       const char *file_contents,
					       gpointer callback_data);
typedef struct NautilusReadFileHandle NautilusReadFileHandle;

char *                  nautilus_format_uri_for_display       (const char                *uri);
char *                  nautilus_make_uri_from_input          (const char                *location);
char *			nautilus_make_uri_from_shell_arg      (const char 		 *location);

gboolean                nautilus_uri_is_trash                 (const char                *uri);
gboolean		nautilus_uri_is_trash_folder 	      (const char 		 *uri);
gboolean                nautilus_uri_is_in_trash              (const char                *uri);
char *                  nautilus_make_uri_canonical           (const char                *uri);
gboolean                nautilus_uris_match                   (const char                *uri_1,
							       const char                *uri_2);
gboolean                nautilus_uris_match_ignore_fragments  (const char                *uri_1,
							       const char                *uri_2);
char *                  nautilus_uri_get_basename             (const char                *uri);
char *                  nautilus_uri_get_scheme               (const char                *uri);

char *			nautilus_uri_make_full_from_relative  (const char 		 *base_uri,
							       const char 		 *uri);


/* FIXME bugzilla.eazel.com 2424: 
 * This is the same as gnome-libs g_concat_dir_and_file except
 * for handling path == NULL.
 */
char *                  nautilus_make_path                    (const char                *path,
							       const char                *name);



/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *                  nautilus_get_user_directory           (void);
char *                  nautilus_get_user_main_directory      (void);
char *                  nautilus_get_desktop_directory        (void);
char *                  nautilus_get_pixmap_directory         (void);



/* See if the user_main_directory exists. This should be called before
 * nautilus_get_user_main_directory, which creates the directory.
 */
gboolean                nautilus_user_main_directory_exists   (void);



/* Convenience routine to test if a string is a remote URI. */
gboolean                nautilus_is_remote_uri                (const char                *uri);



/* A version of gnome's gnome_pixmap_file that works for the nautilus prefix.
 * Otherwise similar to gnome_pixmap_file in that it checks to see if the file
 * exists and returns NULL if it doesn't.
 */
/* FIXME bugzilla.eazel.com 2425: 
 * We might not need this once we get on gnome-libs 2.0 which handles
 * gnome_pixmap_file better, using GNOME_PATH.
 */
char *                  nautilus_pixmap_file                  (const char                *partial_path);



/* Read an entire file at once with gnome-vfs. */
GnomeVFSResult          nautilus_read_entire_file             (const char                *uri,
							       int                       *file_size,
							       char                     **file_contents);
NautilusReadFileHandle *nautilus_read_entire_file_async       (const char                *uri,
							       NautilusReadFileCallback   callback,
							       gpointer                   callback_data);
NautilusReadFileHandle *nautilus_read_file_async              (const char                *uri,
							       NautilusReadFileCallback   callback,
							       NautilusReadMoreCallback   read_more_callback,
							       gpointer                   callback_data);
void                    nautilus_read_file_cancel             (NautilusReadFileHandle    *handle);



/* Convenience routine for simple file copying using text-based uris */
GnomeVFSResult          nautilus_copy_uri_simple              (const char                *source_uri,
							       const char                *dest_uri);



/* gnome-vfs cover to make a directory and parents */
GnomeVFSResult          nautilus_make_directory_and_parents   (GnomeVFSURI               *uri,
							       guint                      permissions);

/* Returns the build time stamp the Nautilus binary.
 * This is useful to be able to tell builds apart.
 * A return value of NULL means unknown time stamp.
 */
char *                  nautilus_get_build_time_stamp         (void);

/* Return an allocated file name that is guranteed to be unique. */
char *                  nautilus_unique_temporary_file_name   (void);
char *                  nautilus_find_file_in_gnome_path      (char                      *file);
GList *                 nautilus_find_all_files_in_gnome_path (char                      *file);

#endif /* NAUTILUS_FILE_UTILITIES_H */
