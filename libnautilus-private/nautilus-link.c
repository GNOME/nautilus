/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.c: xml-based link files.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-link.h"
#include "nautilus-link-historical.h"
#include "nautilus-link-desktop-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>

const char *get_uri_mime_type_full (const gchar *uri_path);

const char *
get_uri_mime_type_full (const gchar *uri_path)
{
	const gchar *retval;
	GnomeVFSURI *uri;

	uri = gnome_vfs_uri_new (uri_path);
	retval = gnome_vfs_get_mime_type (uri);
	gnome_vfs_uri_unref (uri);

	return retval;
}

gboolean
nautilus_link_local_create (const char *directory_path,
			    const char *name,
			    const char *image,
			    const char *target_uri,
			    const GdkPoint *point,
			    NautilusLinkType type)
{
	gboolean retval;

	retval = nautilus_link_desktop_file_local_create (directory_path,
							  name, image,
							  target_uri, point,
							  type);

	return retval;
}

gboolean
nautilus_link_local_set_icon (const char *path, const char *icon_name)
{
	const gchar *mime_type;
	gboolean retval;
	NautilusFile *file;
	GList *attributes;

	mime_type = get_uri_mime_type_full (path);
	retval = FALSE;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_set_icon (path, icon_name);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_set_icon (path, icon_name);
	}

	file = nautilus_file_get (path);
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_invalidate_attributes (file, attributes);
	nautilus_file_unref (file);
	g_list_free (attributes);
	
	return retval;
}

gboolean
nautilus_link_local_set_link_uri (const char *path, const char *link_uri)
{
	const gchar *mime_type;
	gboolean retval;
	NautilusFile *file;
	GList *attributes;

	mime_type = get_uri_mime_type_full (path);
	retval = FALSE;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_set_link_uri (path, link_uri);
	}
	/* FIXME: May want to implement this for desktop files too */
	
	file = nautilus_file_get (path);
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_invalidate_attributes (file, attributes);
	nautilus_file_unref (file);
	g_list_free (attributes);

	return retval;
}

gboolean
nautilus_link_local_set_type (const char *path,
			      NautilusLinkType type)
{
	const gchar *mime_type;
	gboolean retval;

	mime_type = get_uri_mime_type_full (path);
	retval = FALSE;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_set_type (path, type);
	} 
	/* FIXME: May want to implement this for desktop files too */

	return retval;
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_local_get_additional_text (const char *path)
{
	const gchar *mime_type;
	gchar *retval;

	mime_type = get_uri_mime_type_full (path);
	retval = NULL;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_get_additional_text (path);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_get_additional_text (path);
	}

	return retval;
}

/* Returns the link uri associated with a link file. */
char *
nautilus_link_local_get_link_uri (const char *path)
{
	const gchar *mime_type;
	gchar *retval;

	mime_type = get_uri_mime_type_full (path);
	retval = NULL;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_get_link_uri (path);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_get_link_uri (path);
	}

	return retval;
}

/* Returns the link type of the link file. */
NautilusLinkType
nautilus_link_local_get_link_type (const char *path)
{
	const gchar *mime_type;
 	NautilusLinkType retval;

	mime_type = get_uri_mime_type_full (path);
	retval = NAUTILUS_LINK_GENERIC;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_get_link_type (path);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_get_link_type (path);
	}

	return retval;
}

char *
nautilus_link_get_link_uri_given_file_contents (const char *file_contents,
						int file_size)
{
	const gchar *mime_type;
	gchar *retval;

	mime_type = gnome_vfs_get_mime_type_for_data (file_contents, file_size);
	retval = NULL;

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_get_link_uri_given_file_contents (file_contents, file_size);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_get_link_uri_given_file_contents (file_contents, file_size);
	}

	return retval;
}

char *
nautilus_link_get_link_name_given_file_contents (const char *file_contents,
						int file_size)
{
	const gchar *mime_type;
	gchar *retval;

	mime_type = gnome_vfs_get_mime_type_for_data (file_contents, file_size);
	retval = NULL;

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = NULL;
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_get_link_name_given_file_contents (file_contents, file_size);
	}

	return retval;
}

char *
nautilus_link_get_link_icon_given_file_contents (const char *file_contents,
						int file_size)
{
	const gchar *mime_type;
	gchar *retval;

	mime_type = gnome_vfs_get_mime_type_for_data (file_contents, file_size);
	retval = NULL;

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_get_link_icon_given_file_contents (file_contents, file_size);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_get_link_icon_given_file_contents (file_contents, file_size);
	}

	return retval;
}

gboolean
nautilus_link_local_is_volume_link (const char *path)
{
	const gchar *mime_type;
	gboolean retval;

	mime_type = get_uri_mime_type_full (path);
	retval = FALSE;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_is_volume_link (path);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_is_volume_link (path);
	}

	return retval;
}

gboolean
nautilus_link_local_is_home_link (const char *path)
{
	const gchar *mime_type;
	gboolean retval;

	mime_type = get_uri_mime_type_full (path);
	retval = FALSE;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_is_home_link (path);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_is_home_link (path);
	}

	return retval;
}

gboolean
nautilus_link_local_is_trash_link (const char *path)
{
	const gchar *mime_type;
	gboolean retval;

	mime_type = get_uri_mime_type_full (path);
	retval = FALSE;

	if (mime_type == NULL) {
		return retval;
	}

	if (strcmp (mime_type, "application/x-nautilus-link") == 0) {
		retval = nautilus_link_historical_local_is_trash_link (path);
	} else if (strcmp (mime_type, "application/x-gnome-app-info") == 0) {
		retval = nautilus_link_desktop_file_local_is_trash_link (path);
	}

	return retval;
}


void
nautilus_link_local_create_from_gnome_entry (GnomeDesktopEntry *entry, const char *dest_path, const GdkPoint *position)
{
	nautilus_link_desktop_file_local_create_from_gnome_entry (entry, dest_path, position);
}
