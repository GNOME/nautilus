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

#include <gnome.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>


#include "nautilus-file-utilities.h"

const char* const nautilus_user_directory_name = ".gnomad";
const unsigned default_nautilus_directory_mode = 0755;


/**
 * nautilus_make_path:
 * 
 * Make a path name from a base path and name. The base path
 * can end with or without a separator character.
 *
 * Return value: the combined path name.
 **/
gchar * 
nautilus_make_path(const gchar *path, const gchar* name)
{
    	gboolean insert_separator;
    	gint     path_length;
	gchar	*result;

	path_length = strlen(path);
    	insert_separator = path_length > 0 && 
    			   strlen(name) > 0 && 
    			   path[path_length - 1] != G_DIR_SEPARATOR;

    	if (insert_separator)
    	{
    		result = g_strconcat(path, G_DIR_SEPARATOR_S, name, NULL);
    	} 
    	else
    	{
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
const gchar *
nautilus_user_directory()
{
	static gchar *user_directory = NULL;

	if (user_directory == NULL)
	{
		user_directory = nautilus_make_path(g_get_home_dir(),
						    nautilus_user_directory_name);

		if (!g_file_exists(user_directory))
		{
			mkdir(user_directory, default_nautilus_directory_mode);
		}

	}

	if (!g_file_test(user_directory, G_FILE_TEST_ISDIR))
	{
		/* Bad news, directory still isn't there.
		 * Report this to user somehow. 
		 */
		g_assert_not_reached();
	}

	return user_directory;
}
