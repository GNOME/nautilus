/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-actions.h - uri-specific versions of mime action functions

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>
#include "nautilus-mime-actions.h"

#include <libgnomevfs/gnome-vfs.h>
#include "nautilus-lib-self-check-functions.h"


/* FIXME: temporary hack */
static char *
get_mime_type_from_uri_hack (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;
	char *mime_type;

	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (uri, file_info,
					  GNOME_VFS_FILE_INFO_GETMIMETYPE
					  | GNOME_VFS_FILE_INFO_FASTMIMETYPE
		  			  | GNOME_VFS_FILE_INFO_FOLLOWLINKS, NULL);
	if (result != GNOME_VFS_OK) {
		return NULL;
	}

	mime_type = g_strdup (file_info->mime_type);

	gnome_vfs_file_info_unref (file_info);

	return mime_type;
}


GnomeVFSMimeAction *
nautilus_mime_get_default_action_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	GnomeVFSMimeAction *result;

	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_default_action (mime_type);
	g_free (mime_type);

	return result;
}


GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	GnomeVFSMimeApplication *result;
	
	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_default_application (mime_type);
	g_free (mime_type);

	return result;
}

OAF_ServerInfo *
nautilus_mime_get_default_component_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	OAF_ServerInfo *result;

	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_default_component (mime_type);
	g_free (mime_type);

	return result;
}


GList *
nautilus_mime_get_short_list_applications_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	GList *result;

	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_short_list_applications (mime_type);
	g_free (mime_type);

	return result;
}

GList *
nautilus_mime_get_short_list_components_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	GList *result;

	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_short_list_components (mime_type);
	g_free (mime_type);

	return result;
}

GList *
nautilus_mime_get_all_applications_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	GList *result;

	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_all_applications (mime_type);

	/* Hack within a hack: at the moment the short list is sometimes
	 * populated even though the "full" list isn't.
	 */
	if (result == NULL) {
		result = gnome_vfs_mime_get_short_list_applications (mime_type);
	}
	g_free (mime_type);

	return result;
}

GList *
nautilus_mime_get_all_components_for_uri (const char *uri)
{
	/* FIXME: Temporary hack for testing */
	char *mime_type;
	GList *result;

	mime_type = get_mime_type_from_uri_hack (uri);
	result = gnome_vfs_mime_get_all_components (mime_type);

	/* Hack within a hack: at the moment the short list is sometimes
	 * populated even though the "full" list isn't.
	 */
	if (result == NULL) {
		result = gnome_vfs_mime_get_short_list_components (mime_type);
	}
	
	g_free (mime_type);

	

	return result;
}



void
nautilus_mime_set_default_action_type_for_uri (const char             *mime_type,
					       GnomeVFSMimeActionType  action_type)
{
	return;
}


void
nautilus_mime_set_default_application_for_uri (const char              *mime_type,
					       GnomeVFSMimeApplication *application)
{
	return;
}


void
nautilus_mime_set_default_component_for_uri (const char     *mime_type,
					     OAF_ServerInfo *component_iid)
{
	return;
}


void
nautilus_mime_set_short_list_applications_for_uri (const char *mime_type,
						   GList      *applications)
{
	return;
}

void
nautilus_mime_set_short_list_components_for_uri (const char *mime_type,
						 GList      *components)
{
	return;
}

void
nautilus_mime_extend_all_applications_for_uri (const char *mime_type,
					       GList      *applications)
{
	return;
}


void
nautilus_mime_remove_from_all_applications_for_uri (const char *mime_type,
						    GList      *applications)
{
	return;
}
