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
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <stdlib.h>

typedef enum {
	not_link,
	historical,
	desktop
} LinkStyle;

static LinkStyle
get_link_style_for_mime_type (const char *mime_type)
{
	if (mime_type != NULL) {
		if (g_ascii_strcasecmp (mime_type, "application/x-gnome-app-info") == 0) {
			return desktop;
		}
		if (g_ascii_strcasecmp (mime_type, "application/x-nautilus-link") == 0) {
			return historical;
		}
	}
	return not_link;
}

static LinkStyle
get_link_style_for_local_file (const char *path)
{
	LinkStyle type;
	GnomeVFSFileInfo *info;
	char *uri;
	GnomeVFSResult result;

	info = gnome_vfs_file_info_new ();

	uri = gnome_vfs_get_uri_from_local_path (path);
	result = gnome_vfs_get_file_info (uri, info,
					  GNOME_VFS_FILE_INFO_GET_MIME_TYPE
					  | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	g_free (uri);

	if (result == GNOME_VFS_OK
	    && (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) != 0) {
		type = get_link_style_for_mime_type (info->mime_type);
	} else {
		type = not_link;
	}

	gnome_vfs_file_info_unref (info);

	return type;
}

static LinkStyle
get_link_style_for_data (const char *file_contents, int file_size)
{
	return get_link_style_for_mime_type
		(gnome_vfs_get_mime_type_for_data (file_contents, file_size));
}

gboolean
nautilus_link_local_create (const char *directory_path,
			    const char *name,
			    const char *image,
			    const char *target_uri,
			    const GdkPoint *point,
			    NautilusLinkType type)
{
	return nautilus_link_desktop_file_local_create (directory_path,
							name, image,
							target_uri, point,
							type);
}

gboolean
nautilus_link_local_set_icon (const char *path, const char *icon_name)
{
	gboolean result;
	NautilusFile *file;
	GList *attributes;

	switch (get_link_style_for_local_file (path)) {
	case desktop:
		result = nautilus_link_desktop_file_local_set_icon (path, icon_name);
		break;
	case historical:
		result = nautilus_link_historical_local_set_icon (path, icon_name);
		break;
	default:
		result = FALSE;
	}

	file = nautilus_file_get (path);
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_invalidate_attributes (file, attributes);
	nautilus_file_unref (file);
	g_list_free (attributes);

	return result;
}

gboolean
nautilus_link_local_set_link_uri (const char *path, const char *link_uri)
{
	gboolean result;
	NautilusFile *file;
	GList *attributes;

	switch (get_link_style_for_local_file (path)) {
	case desktop:
		/* FIXME: May want to implement this for desktop files too */
		result = FALSE;
		break;
	case historical:
		result = nautilus_link_historical_local_set_link_uri (path, link_uri);
		break;
	default:
		result = FALSE;
	}

	
	file = nautilus_file_get (path);
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_invalidate_attributes (file, attributes);
	nautilus_file_unref (file);
	g_list_free (attributes);

	return result;
}

gboolean
nautilus_link_local_set_type (const char *path,
			      NautilusLinkType type)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		/* FIXME: May want to implement this for desktop files too */
		return FALSE;
	case historical:
		return nautilus_link_historical_local_set_type (path, type);
	default:
		return FALSE;
	}
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_local_get_additional_text (const char *path)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		return nautilus_link_desktop_file_local_get_additional_text (path);
	case historical:
		return nautilus_link_historical_local_get_additional_text (path);
	default:
		return NULL;
	}
}

/* Returns the link uri associated with a link file. */
char *
nautilus_link_local_get_link_uri (const char *path)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		return nautilus_link_desktop_file_local_get_link_uri (path);
	case historical:
		return nautilus_link_historical_local_get_link_uri (path);
	default:
		return NULL;
	}
}

/* Returns the link type of the link file. */
NautilusLinkType
nautilus_link_local_get_link_type (const char *path)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		return nautilus_link_desktop_file_local_get_link_type (path);
	case historical:
		return nautilus_link_historical_local_get_link_type (path);
	default:
		return NAUTILUS_LINK_GENERIC;
	}
}

char *
nautilus_link_get_link_uri_given_file_contents (const char *file_contents,
						int file_size)
{
	switch (get_link_style_for_data (file_contents, file_size)) {
	case desktop:
		return nautilus_link_desktop_file_get_link_uri_given_file_contents (file_contents, file_size);
	case historical:
		return nautilus_link_historical_get_link_uri_given_file_contents (file_contents, file_size);
	default:
		return NULL;
	}
}

char *
nautilus_link_get_link_name_given_file_contents (const char *file_contents,
						 int file_size)
{
	switch (get_link_style_for_data (file_contents, file_size)) {
	case desktop:
		return nautilus_link_desktop_file_get_link_name_given_file_contents (file_contents, file_size);
	case historical:
		return NULL;
	default:
		return NULL;
	}
}

char *
nautilus_link_get_link_icon_given_file_contents (const char *file_contents,
						int file_size)
{
	switch (get_link_style_for_data (file_contents, file_size)) {
	case desktop:
		return nautilus_link_desktop_file_get_link_icon_given_file_contents (file_contents, file_size);
	case historical:
		return nautilus_link_historical_get_link_icon_given_file_contents (file_contents, file_size);
	default:
		return NULL;
	}
}

gboolean
nautilus_link_local_is_volume_link (const char *path)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		return nautilus_link_desktop_file_local_is_volume_link (path);
	case historical:
		return nautilus_link_historical_local_is_volume_link (path);
	default:
		return FALSE;
	}
}

gboolean
nautilus_link_local_is_home_link (const char *path)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		return nautilus_link_desktop_file_local_is_home_link (path);
	case historical:
		return nautilus_link_historical_local_is_home_link (path);
	default:
		return FALSE;
	}
}

gboolean
nautilus_link_local_is_trash_link (const char *path)
{
	switch (get_link_style_for_local_file (path)) {
	case desktop:
		return nautilus_link_desktop_file_local_is_trash_link (path);
	case historical:
		return nautilus_link_historical_local_is_trash_link (path);
	default:
		return FALSE;
	}
}

#if GNOME2_CONVERSION_COMPLETE

void
nautilus_link_local_create_from_gnome_entry (GnomeDesktopEntry *entry,
					     const char *dest_path,
					     const GdkPoint *position)
{
	nautilus_link_desktop_file_local_create_from_gnome_entry (entry, dest_path, position);
}

#endif
