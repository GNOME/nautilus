/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities.h - interface for file manipulation routines.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

/* Recognizing special file names. */
gboolean nautilus_file_name_matches_hidden_pattern   (const char *name_or_relative_uri);
gboolean nautilus_file_name_matches_backup_pattern   (const char *name_or_relative_uri);
gboolean nautilus_file_name_matches_metafile_pattern (const char *name_or_relative_uri);

/* FIXME bugzilla.gnome.org 42424: 
 * This is the same as gnome-libs g_concat_dir_and_file except
 * for handling path == NULL.
 */
char *   nautilus_make_path                          (const char *path,
						      const char *name);
/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *   nautilus_get_user_directory                 (void);
char *   nautilus_get_desktop_directory              (void);
char *   nautilus_get_gmc_desktop_directory          (void);
char *   nautilus_get_pixmap_directory               (void);

/* A version of gnome's gnome_pixmap_file that works for the nautilus prefix.
 * Otherwise similar to gnome_pixmap_file in that it checks to see if the file
 * exists and returns NULL if it doesn't.
 */
/* FIXME bugzilla.gnome.org 42425: 
 * We might not need this once we get on gnome-libs 2.0 which handles
 * gnome_pixmap_file better, using GNOME_PATH.
 */
char *   nautilus_pixmap_file                        (const char *partial_path);

/* Locate a file in either the uers directory or the datadir. */
char *   nautilus_get_data_file_path                 (const char *partial_path);

/* Returns the build time stamp the Nautilus binary.
 * This is useful to be able to tell builds apart.
 * A return value of NULL means unknown time stamp.
 */
char *   nautilus_get_build_time_stamp               (void);

/* Returns the "build message", which provides some information on build
 * context.  May return NULL.
 */
char *   nautilus_get_build_message                  (void);

/* Return an allocated file name that is guranteed to be unique. */
char *   nautilus_unique_temporary_file_name         (void);
char *   nautilus_find_file_in_gnome_path            (char       *file);
GList *  nautilus_find_all_files_in_gnome_path       (char       *file);

#endif /* NAUTILUS_FILE_UTILITIES_H */
