/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities.c - implementation of file manipulation routines.

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

#include <config.h>
#include "nautilus-file-utilities.h"

#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link-set.h"
#include "nautilus-metadata.h"
#include "nautilus-metafile.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#define NAUTILUS_USER_DIRECTORY_NAME ".nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

#define DESKTOP_DIRECTORY_NAME "desktop"
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

#define NAUTILUS_USER_MAIN_DIRECTORY_NAME "Nautilus"

gboolean
nautilus_file_name_matches_hidden_pattern (const char *name_or_relative_uri)
{
	g_return_val_if_fail (name_or_relative_uri != NULL, FALSE);

	return name_or_relative_uri[0] == '.';
}

gboolean
nautilus_file_name_matches_backup_pattern (const char *name_or_relative_uri)
{
	g_return_val_if_fail (name_or_relative_uri != NULL, FALSE);

	return eel_str_has_suffix (name_or_relative_uri, "~");
}

gboolean
nautilus_file_name_matches_metafile_pattern (const char *name_or_relative_uri)
{
	g_return_val_if_fail (name_or_relative_uri != NULL, FALSE);

	return eel_str_has_suffix (name_or_relative_uri, NAUTILUS_METAFILE_NAME_SUFFIX);
}

/**
 * nautilus_make_path:
 * 
 * Make a path name from a base path and name. The base path
 * can end with or without a separator character.
 *
 * Return value: the combined path name.
 **/
char * 
nautilus_make_path (const char *path, const char* name)
{
    	gboolean insert_separator;
    	int path_length;
	char *result;
	
	path_length = strlen (path);
    	insert_separator = path_length > 0 && 
    			   name[0] != '\0' && 
    			   path[path_length - 1] != G_DIR_SEPARATOR;

    	if (insert_separator) {
    		result = g_strconcat (path, G_DIR_SEPARATOR_S, name, NULL);
    	} else {
    		result = g_strconcat (path, name, NULL);
    	}

	return result;
}

/**
 * nautilus_get_user_directory:
 * 
 * Get the path for the directory containing nautilus settings.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_user_directory (void)
{
	char *user_directory = NULL;

	user_directory = nautilus_make_path (g_get_home_dir (),
					     NAUTILUS_USER_DIRECTORY_NAME);

	if (!g_file_exists (user_directory)) {
		mkdir (user_directory, DEFAULT_NAUTILUS_DIRECTORY_MODE);
		/* FIXME bugzilla.eazel.com 1286: 
		 * How should we handle the case where this mkdir fails? 
		 * Note that nautilus_application_startup will refuse to launch if this 
		 * directory doesn't get created, so that case is OK. But the directory 
		 * could be deleted after Nautilus was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
	}

	return user_directory;
}

/**
 * nautilus_get_desktop_directory:
 * 
 * Get the path for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_desktop_directory (void)
{
	char *desktop_directory, *user_directory;

	user_directory = nautilus_get_user_directory ();
	desktop_directory = nautilus_make_path (user_directory, DESKTOP_DIRECTORY_NAME);
	g_free (user_directory);

	if (!g_file_exists (desktop_directory)) {
		mkdir (desktop_directory, DEFAULT_DESKTOP_DIRECTORY_MODE);
		/* FIXME bugzilla.eazel.com 1286: 
		 * How should we handle the case where this mkdir fails? 
		 * Note that nautilus_application_startup will refuse to launch if this 
		 * directory doesn't get created, so that case is OK. But the directory 
		 * could be deleted after Nautilus was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
	}

	return desktop_directory;
}

/**
  * nautilus_user_main_directory_exists:
  *
  * returns true if the user directory exists.  This must be called
  * before nautilus_get_user_main_directory, which creates it if necessary
  *
  **/
gboolean
nautilus_user_main_directory_exists(void)
{
	gboolean directory_exists;
	char *main_directory;
	
	main_directory = g_strdup_printf ("%s/%s",
					g_get_home_dir(),
					NAUTILUS_USER_MAIN_DIRECTORY_NAME);
	directory_exists = g_file_exists(main_directory);
	g_free(main_directory);
	return directory_exists;
}


/**
 * nautilus_get_user_main_directory:
 * 
 * Get the path for the user's main Nautilus directory.  
 * Usually ~/Nautilus
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_user_main_directory (void)
{
	char *user_main_directory = NULL;
	GnomeVFSResult result;
	char *destination_directory_uri_text;
	GnomeVFSURI *destination_directory_uri;
	GnomeVFSURI *destination_uri;
	
	user_main_directory = g_strdup_printf ("%s/%s",
					       g_get_home_dir(),
					       NAUTILUS_USER_MAIN_DIRECTORY_NAME);
												
	if (!g_file_exists (user_main_directory)) {			
		destination_directory_uri_text = gnome_vfs_get_uri_from_local_path (g_get_home_dir());
		destination_directory_uri = gnome_vfs_uri_new (destination_directory_uri_text);
		g_free (destination_directory_uri_text);
		destination_uri = gnome_vfs_uri_append_file_name (destination_directory_uri, 
								  NAUTILUS_USER_MAIN_DIRECTORY_NAME);
		gnome_vfs_uri_unref (destination_directory_uri);
		
		result = gnome_vfs_make_directory_for_uri (destination_uri,
						 GNOME_VFS_PERM_USER_ALL
						 | GNOME_VFS_PERM_GROUP_ALL
						 | GNOME_VFS_PERM_OTHER_READ);

		/* FIXME bugzilla.eazel.com 1286: 
		 * How should we handle error codes returned from gnome_vfs_xfer_uri? 
		 * Note that nautilus_application_startup will refuse to launch if this 
		 * directory doesn't get created, so that case is OK. But the directory 
		 * could be deleted after Nautilus was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
		gnome_vfs_uri_unref (destination_uri);

		/* If this fails to create the directory, nautilus_application_startup will
		 * notice and refuse to launch.
		 */
		
		/* install the default link sets */
		nautilus_link_set_install (user_main_directory, "apps");
		nautilus_link_set_install (user_main_directory, "home");
	}

	return user_main_directory;
}

/**
 * nautilus_get_pixmap_directory
 * 
 * Get the path for the directory containing Nautilus pixmaps.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_pixmap_directory (void)
{
	return g_strdup (DATADIR "/pixmaps/nautilus");
}

/* FIXME bugzilla.eazel.com 2423: 
 * Callers just use this and dereference so we core dump if
 * pixmaps are missing. That is lame.
 */
char *
nautilus_pixmap_file (const char *partial_path)
{
	char *path;

	/* Look for a non-GPL Eazel logo version. */
	path = nautilus_make_path (DATADIR "/pixmaps/nautilus/eazel-logos", partial_path);
	if (g_file_exists (path)) {
		return path;
	}
	g_free (path);

	/* Look for a GPL version. */
	path = nautilus_make_path (DATADIR "/pixmaps/nautilus", partial_path);
	if (g_file_exists (path)) {
		return path;
	}
	g_free (path);

	return NULL;
}

char *
nautilus_unique_temporary_file_name (void)
{
	const char *prefix = "/tmp/nautilus-temp-file";
	char *file_name;
	static guint count = 1;

	file_name = g_strdup_printf ("%sXXXXXX", prefix);

	if (mktemp (file_name) != file_name) {
		g_free (file_name);
		file_name = g_strdup_printf ("%s-%d-%d", prefix, count++, getpid ());
	}

	return file_name;
}

char *
nautilus_get_build_time_stamp (void)
{
#ifdef EAZEL_BUILD_TIMESTAMP
	return g_strdup (EAZEL_BUILD_TIMESTAMP);
#else
	return NULL;
#endif
}

char *
nautilus_get_build_message (void)
{
#ifdef NAUTILUS_BUILD_MESSAGE
	return g_strdup (NAUTILUS_BUILD_MESSAGE);
#else
	return NULL;
#endif
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_utilities (void)
{
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
