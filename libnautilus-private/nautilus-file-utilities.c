/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities..c - implementation of file manipulation routines.

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

#include <config.h>
#include "nautilus-file-utilities.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "nautilus-file.h"
#include "nautilus-link-set.h"
#include "nautilus-metadata.h"


#include "nautilus-string.h"
#include <libgnomevfs/gnome-vfs-utils.h>

#define NAUTILUS_USER_DIRECTORY_NAME ".nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

#define DESKTOP_DIRECTORY_NAME "desktop"
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

#define NAUTILUS_USER_MAIN_DIRECTORY_NAME "Nautilus"

#define DEFAULT_SCHEME "file://"

/**
 * nautilus_format_uri_for_display:
 *
 * Filter, modify, unescape and change URIs to make them appropriate
 * to display to users.
 *
 * @uri: a URI
 *
 * returns a g_malloc'd string
 **/
char *
nautilus_format_uri_for_display (const char *uri) 
{
	gchar *toreturn, *unescaped;

	g_assert (uri != NULL);

	unescaped = gnome_vfs_unescape_for_display (uri);
	
	/* Remove file:// from the beginning */
	if (nautilus_str_has_prefix (uri, DEFAULT_SCHEME)) {
		toreturn = strdup (unescaped + sizeof (DEFAULT_SCHEME) - 1);
	} else {
		toreturn = strdup (unescaped);
	}
	
	g_free (unescaped);

	return toreturn;
}

/**
 * nautilus_make_uri_from_input:
 *
 * Takes a user input path/URI and makes a valid URI
 * out of it
 *
 * @location: a possibly mangled "uri"
 *
 * returns a newly allocated uri
 *
 **/
char *
nautilus_make_uri_from_input (const char *location)
{
	gchar *toreturn;

	/* FIXME: add escaping logic to this function */
	if (location[0] == '/') {
		toreturn = g_strconcat (DEFAULT_SCHEME, location, NULL);
	} else {
		toreturn = strdup (location);
	}

	return toreturn;
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
nautilus_make_path(const char *path, const char* name)
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

/* FIXME bugzilla.eazel.com 1117: Change file-utilities.c to always create user
 * directories if needed. See bug for details.
 */

/**
 * nautilus_get_user_directory:
 * 
 * Get the path for the directory containing nautilus settings.
 *
 * Return value: the directory path.
 **/
const char *
nautilus_get_user_directory (void)
{
	static char *user_directory = NULL;

	if (user_directory == NULL) {
		user_directory = nautilus_make_path (g_get_home_dir (),
						     NAUTILUS_USER_DIRECTORY_NAME);

		if (!g_file_exists (user_directory)) {
			mkdir (user_directory, DEFAULT_NAUTILUS_DIRECTORY_MODE);
		}

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
const char *
nautilus_get_desktop_directory (void)
{
	static char *desktop_directory = NULL;

	if (desktop_directory == NULL) {
		desktop_directory = nautilus_make_path (nautilus_get_user_directory (),
							DESKTOP_DIRECTORY_NAME);
		if (!g_file_exists (desktop_directory)) {
			mkdir (desktop_directory, DEFAULT_DESKTOP_DIRECTORY_MODE);
		}

	}

	return desktop_directory;
}

/**
 * nautilus_get_user_main_directory:
 * 
 * Get the path for the user's main Nautilus directory.  
 * Usually ~/Nautilus
 *
 * Return value: the directory path.
 **/
const char *
nautilus_get_user_main_directory (void)
{
	static char *user_main_directory = NULL;
	NautilusFile *file;
	char *command, *file_uri, *image_uri, *temp_str;

	
	if (user_main_directory == NULL)
	{
		user_main_directory = g_strdup_printf ("%s/%s",
							g_get_home_dir(),
							NAUTILUS_USER_MAIN_DIRECTORY_NAME);
												
		if (!g_file_exists (user_main_directory)) {
			/* FIXME bugzilla.eazel.com 1285: 
			 * Is it OK to use cp like this? What about quoting the parameters? 
			 */
			command = g_strdup_printf ("cp -R %s %s",
						   NAUTILUS_DATADIR "/top",
						   user_main_directory);

			/* FIXME bugzilla.eazel.com 1286: 
			 * Is a g_warning good enough here? This seems like a big problem. 
			 */
			if (system (command) != 0) {
				g_warning ("could not execute '%s'.  Make sure you typed 'make install'", 
					   command);
			}
			
			g_free (command);
		
			/* assign a custom image for the directory icon */
			file_uri = nautilus_get_uri_from_local_path (user_main_directory);
			temp_str = nautilus_pixmap_file ("nautilus-logo.png");
			image_uri = nautilus_get_uri_from_local_path (temp_str);
			g_free (temp_str);
			
			file = nautilus_file_get (file_uri);
			g_free (file_uri);
			if (file != NULL) {
				nautilus_file_set_metadata (file,
							    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
							    NULL,
							    image_uri);
				nautilus_file_unref (file);
			}

			/* now do the same for the about file */
			temp_str = g_strdup_printf ("%s/About.html", user_main_directory);
			file_uri = nautilus_get_uri_from_local_path (temp_str);
			g_free (temp_str);
			
			file = nautilus_file_get (file_uri);
			if (file != NULL) {
				nautilus_file_set_metadata (file,
							    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
							    NULL,
							    image_uri);
				nautilus_file_unref (file);
			}
			g_free (file_uri);

			g_free (image_uri);

			/* install the default link set */
			nautilus_link_set_install(user_main_directory, "apps");
			/*
			nautilus_link_set_install(user_main_directory, "search_engines");
			*/
		}
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
const char *
nautilus_get_pixmap_directory (void)
{
	return DATADIR "/pixmaps/nautilus";
}

/**
 * nautilus_get_local_path_from_uri:
 * 
 * Return a local path for a file:// URI.
 *
 * Return value: the local path or NULL on error.
 **/
char *
nautilus_get_local_path_from_uri (const char *uri)
{
	char *result, *unescaped_uri;

	if (uri == NULL) {
		return NULL;
	}

	unescaped_uri = gnome_vfs_unescape_string (uri, "/");

	if (unescaped_uri == NULL) {
		return NULL;
	}

	if (nautilus_str_has_prefix (unescaped_uri, "file://")) {
		result = g_strdup (unescaped_uri+7);
	} else if (unescaped_uri[0] == '/') {
		result = g_strdup (unescaped_uri);
	} else {
		result = NULL;
	}

	g_free (unescaped_uri);

	return result;
}

/**
 * nautilus_get_uri_from_local_path:
 * 
 * Return a file:// URI for a local path.
 *
 * Return value: the URI (NULL for some bad errors).
 **/
char *
nautilus_get_uri_from_local_path (const char *local_path)
{
	char *escaped_path, *result;

	g_return_val_if_fail (local_path != NULL, NULL);
	g_return_val_if_fail (local_path[0] == '/', NULL);

	escaped_path = gnome_vfs_escape_string (local_path, GNOME_VFS_URI_UNSAFE_PATH);
	result = g_strconcat ("file://", escaped_path, NULL);
	g_free (escaped_path);
	return result;
}

/* FIXME: Callers just use this and dereference so we core dump if
 * pixmaps are missing. That is lame.
 */
char *
nautilus_pixmap_file (const char *partial_path)
{
	char *path;

	path = nautilus_make_path (DATADIR "/pixmaps/nautilus", partial_path);
	if (g_file_exists (path)) {
		return path;
	} else {
		g_free (path);
		return NULL;
	}
}
