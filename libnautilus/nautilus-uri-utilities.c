/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-uri-utilities..c - implementation of uri manipulation routines.

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

#include "nautilus-uri-utilities.h"

#include <libgnomevfs/gnome-vfs.h>

/**
 * nautilus_path_known_not_to_exist:
 *
 * Check whether a uri represents a path known in advance not to exist.
 * A return value of FALSE does not guarantee that the path is valid,
 * but a return value of TRUE does guarantee that the path is not valid.
 * Use this to make quick rejection decisions.
 * @uri: The uri to check.
 * 
 * Return value: TRUE if inspection of uri reveals that it is invalid,
 * FALSE otherwise.
 **/
gboolean
nautilus_path_known_not_to_exist (const char *uri_string)
{
	GnomeVFSURI *uri;
	GnomeVFSFileInfo file_info;
	GnomeVFSResult result;

	g_return_val_if_fail(uri_string != NULL, TRUE);

	/* Handle empty string here since gnome_vfs_uri_new chokes on it. */
	if (uri_string[0] == 0)
		return TRUE;

	uri = gnome_vfs_uri_new(uri_string);

	if (uri == NULL)
		return TRUE;

	/* Since we want only instant results, don't check remote uri's */
	if (!gnome_vfs_uri_is_local(uri))
		return FALSE;

	result = gnome_vfs_get_file_info_uri(uri, 
					     &file_info,
					     GNOME_VFS_FILE_INFO_DEFAULT,
					     NULL);

	return result != GNOME_VFS_OK;	
}