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

#include "nautilus-file-utilities.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

const char* const nautilus_user_directory_name = ".nautilus";
const char* const nautilus_user_top_directory_name = "top";
const unsigned default_nautilus_directory_mode = 0755;



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



/**
 * nautilus_user_directory:
 * 
 * Get the path for the directory containing nautilus settings.
 *
 * Return value: the directory path.
 **/
const char *
nautilus_user_directory()
{
	static char *user_directory;

	if (user_directory == NULL)
	{
		user_directory = nautilus_make_path (g_get_home_dir(),
						     nautilus_user_directory_name);

		if (!g_file_exists (user_directory))
		{
			mkdir (user_directory, default_nautilus_directory_mode);
		}

	}

	if (!g_file_test (user_directory, G_FILE_TEST_ISDIR))
	{
		/* Bad news, directory still isn't there.
		 * FIXME: Report this to user somehow. 
		 */
		g_assert_not_reached();
	}

	return user_directory;
}

/**
 * nautilus_user_top_directory:
 * 
 * Get the path for the user's top directory.  
 * Usually ~/.nautilus/top
 *
 * Return value: the directory path.
 **/
const char *
nautilus_user_top_directory (void)
{
	static char *user_top_directory;

	if (user_top_directory == NULL)
	{
		const char * user_directory;
		
		user_directory = nautilus_user_directory ();

		g_assert (user_directory != NULL);

		user_top_directory = nautilus_make_path (user_directory,
							 nautilus_user_top_directory_name);

		if (!g_file_exists (user_top_directory))
		{
			/* FIXME: Hack the prefix for now */
			const char *gnome_prefix = "/gnome";

			GString	   *src;
			GString	   *dst;
			GString	   *command;


			src = g_string_new (gnome_prefix);
			g_string_append (src, "/share/nautilus/top");

			dst = g_string_new (user_top_directory);

			command = g_string_new ("cp -R ");
			g_string_append (command, src->str);
			g_string_append (command, " ");
			g_string_append (command, dst->str);

			if (system (command->str) != 0)
			{
				g_warning ("could not execute '%s'.  Make sure you typed 'make install'\n", 
					   command->str);
			}
			
			g_string_free (src, TRUE);
			g_string_free (dst, TRUE);
			g_string_free (command, TRUE);
		}

	}

	if (!g_file_test (user_top_directory, G_FILE_TEST_ISDIR))
	{
		/* Bad news, directory still isn't there.
		 * FIXME: Report this to user somehow. 
		 */
		g_assert_not_reached();

	}

	return user_top_directory;
}
