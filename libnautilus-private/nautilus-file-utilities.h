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

#include <gio/gio.h>

#define NAUTILUS_SAVED_SEARCH_EXTENSION ".savedSearch"
#define NAUTILUS_SAVED_SEARCH_MIMETYPE "application/x-gnome-saved-search"

/* Recognizing special file names. */
gboolean nautilus_file_name_matches_hidden_pattern   (const char *name_or_relative_uri);
gboolean nautilus_file_name_matches_backup_pattern   (const char *name_or_relative_uri);

/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *   nautilus_get_xdg_dir                        (const char *type);
char *   nautilus_get_user_directory                 (void);
char *   nautilus_get_desktop_directory              (void);
GFile *  nautilus_get_desktop_location               (void);
char *   nautilus_get_desktop_directory_uri          (void);
char *   nautilus_get_home_directory_uri             (void);
gboolean nautilus_is_desktop_directory_file          (GFile *dir,
						      const char *filename);
gboolean nautilus_is_root_directory                  (GFile *dir);
gboolean nautilus_is_desktop_directory               (GFile *dir);
gboolean nautilus_is_home_directory                  (GFile *dir);
gboolean nautilus_is_home_directory_file             (GFile *dir,
						      const char *filename);
char *   nautilus_get_gmc_desktop_directory          (void);
char *   nautilus_get_pixmap_directory               (void);

gboolean nautilus_should_use_templates_directory     (void);
char *   nautilus_get_templates_directory            (void);
char *   nautilus_get_templates_directory_uri        (void);
void     nautilus_create_templates_directory         (void);

char *   nautilus_get_searches_directory             (void);

char *	 nautilus_compute_title_for_location	     (GFile *file);

/* This function returns something that needs to be freed with g_free,
 * is not NULL, but is not garaunteed to exist */
char *   nautilus_get_desktop_directory_uri_no_create (void);

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

/* Return an allocated file name that is guranteed to be unique, but
 * tries to make the name readable to users.
 * This isn't race-free, so don't use for security-related things
 */
char *   nautilus_ensure_unique_file_name            (const char *directory_uri,
						      const char *base_name,
			                              const char *extension);
char *   nautilus_unique_temporary_file_name         (void);

GFile *  nautilus_find_existing_uri_in_hierarchy     (GFile *location);

GFile *
nautilus_find_file_insensitive (GFile *parent, const gchar *name);

#endif /* NAUTILUS_FILE_UTILITIES_H */
