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

#define NAUTILUS_USER_DIRECTORY_NAME ".nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)

#define DESKTOP_DIRECTORY_NAME "desktop"
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

#define NAUTILUS_USER_MAIN_DIRECTORY_NAME "Nautilus"



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
    	gint     path_length;
	char	*result;

	path_length = strlen (path);
    	insert_separator = path_length > 0 && 
    			   name[0] != '\0' > 0 && 
    			   path[path_length - 1] != G_DIR_SEPARATOR;

    	if (insert_separator) {
    		result = g_strconcat(path, G_DIR_SEPARATOR_S, name, NULL);
    	} else {
    		result = g_strconcat(path, name, NULL);
    	}

	return result;
}

/* FIXME bugzilla.eazel.com 1117: Change file-utilities.c to always create user
 * directorie if needed.  See bug for detail
 */

/**
 * nautilus_user_directory:
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
 * nautilus_desktop_directory:
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
 * nautilus_user_main_directory:
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
	
	if (user_main_directory == NULL)
	{
		user_main_directory = g_strdup_printf ("%s/%s",
							g_get_home_dir(),
							NAUTILUS_USER_MAIN_DIRECTORY_NAME);
												
		if (!g_file_exists (user_main_directory)) {
			char	   *src;
			char	   *command;
			char	   *file_uri, *image_uri, *temp_str;

			src = gnome_datadir_file ("nautilus/top");

			/* FIXME: Is it OK to use cp like this? What about quoting the parameters? */
			command = g_strdup_printf ("cp -R %s %s", src, user_main_directory);

			/* FIXME: Is a g_warning good enough here? This seems like a big problem. */
			if (system (command) != 0) {
				g_warning ("could not execute '%s'.  Make sure you typed 'make install'", 
					   command);
			}
			
			g_free (src);
			g_free (command);
		
			/* assign a custom image for the directory icon */
			file_uri = g_strdup_printf("file://%s", user_main_directory);
			temp_str = gnome_pixmap_file ("nautilus/nautilus-logo.png");
			image_uri = g_strdup_printf("file://%s", temp_str);
			g_free(temp_str);
			
			file = nautilus_file_get (file_uri);
			if (file != NULL) {
				nautilus_file_set_metadata (file,
							    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
							    NULL,
							    image_uri);
				nautilus_file_unref (file);
			}
			g_free (file_uri);

			/* now do the same for the about file */
			file_uri = g_strdup_printf("file://%s/About.html", user_main_directory);
			
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
