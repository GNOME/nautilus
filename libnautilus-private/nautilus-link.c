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
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
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
get_link_style_for_local_file (const char *uri, GnomeVFSFileInfo *opt_info)
{
	LinkStyle type;
	GnomeVFSResult result;
	GnomeVFSFileInfo *info;

	if (!(info = opt_info)) {
		info = gnome_vfs_file_info_new ();

		result = gnome_vfs_get_file_info (uri, info,
						  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
		if (result != GNOME_VFS_OK) {
			gnome_vfs_file_info_unref (info);
			info = NULL;
		}
	}

	if (info && info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) {
		type = get_link_style_for_mime_type (info->mime_type);
	} else {
		type = not_link;
	}

	if (!opt_info && info)
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
nautilus_link_local_create (const char *directory_uri,
			    const char *name,
			    const char *image,
			    const char *target_uri,
			    const GdkPoint *point,
			    int screen,
			    NautilusLinkType type)
{
	return nautilus_link_desktop_file_local_create (directory_uri,
							name, image,
							target_uri, 
							point, screen,
							type);
}

gboolean
nautilus_link_local_set_icon (const char *uri, const char *icon_name)
{
	gboolean result;
	NautilusFile *file;

	switch (get_link_style_for_local_file (uri, NULL)) {
	case desktop:
		result = nautilus_link_desktop_file_local_set_icon (uri, icon_name);
		break;
	case historical:
		result = nautilus_link_historical_local_set_icon (uri, icon_name);
		break;
	default:
		result = FALSE;
	}

	file = nautilus_file_get (uri);
	nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_unref (file);

	return result;
}

gboolean
nautilus_link_local_set_link_uri (const char *uri, const char *link_uri)
{
	gboolean result;
	NautilusFile *file;

	switch (get_link_style_for_local_file (uri, NULL)) {
	case desktop:
		/* FIXME: May want to implement this for desktop files too */
		result = FALSE;
		break;
	case historical:
		result = nautilus_link_historical_local_set_link_uri (uri, link_uri);
		break;
	default:
		result = FALSE;
	}

	
	file = nautilus_file_get (uri);
	nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_unref (file);

	return result;
}

gboolean
nautilus_link_local_set_type (const char *uri,
			      NautilusLinkType type)
{
	switch (get_link_style_for_local_file (uri, NULL)) {
	case desktop:
		/* FIXME: May want to implement this for desktop files too */
		return FALSE;
	case historical:
		return nautilus_link_historical_local_set_type (uri, type);
	default:
		return FALSE;
	}
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_local_get_additional_text (const char *uri)
{
	switch (get_link_style_for_local_file (uri, NULL)) {
	case desktop:
		return nautilus_link_desktop_file_local_get_additional_text (uri);
	case historical:
		return nautilus_link_historical_local_get_additional_text (uri);
	default:
		return NULL;
	}
}

/* Returns the link uri associated with a link file. */
char *
nautilus_link_local_get_link_uri (const char *uri)
{
	switch (get_link_style_for_local_file (uri, NULL)) {
	case desktop:
		return nautilus_link_desktop_file_local_get_link_uri (uri);
	case historical:
		return nautilus_link_historical_local_get_link_uri (uri);
	default:
		return NULL;
	}
}

/* Returns the link type of the link file. */
NautilusLinkType
nautilus_link_local_get_link_type (const char *uri, GnomeVFSFileInfo *info)
{
	switch (get_link_style_for_local_file (uri, info)) {
	case desktop:
		return nautilus_link_desktop_file_local_get_link_type (uri);
	case historical:
		return nautilus_link_historical_local_get_link_type (uri);
	default:
		return NAUTILUS_LINK_GENERIC;
	}
}

gboolean
nautilus_link_local_is_utf8 (const char *uri,
			     GnomeVFSFileInfo *info)
{
	switch (get_link_style_for_local_file (uri, info)) {
	case desktop:
		return nautilus_link_desktop_file_local_is_utf8 (uri);
	case historical:
	default:
		return FALSE;
	}
}

void
nautilus_link_get_link_info_given_file_contents (const char       *file_contents,
						 int               link_file_size,
						 char            **uri,
						 char            **name,
						 char            **icon,
						 gulong           *drive_id,
						 gulong           *volume_id)
{
	*uri = NULL;
	*name = NULL;
	*icon = NULL;
	*drive_id = 0;
	*volume_id = 0;
	
	switch (get_link_style_for_data (file_contents, link_file_size)) {
	case desktop:
		return nautilus_link_desktop_file_get_link_info_given_file_contents (file_contents, link_file_size, uri, name, icon, drive_id, volume_id);
	case historical:
		*uri = nautilus_link_historical_get_link_uri_given_file_contents (file_contents, link_file_size);
		*icon = nautilus_link_historical_get_link_icon_given_file_contents (file_contents, link_file_size);
		break;
	default:
		return;
	}
}

gboolean
nautilus_link_local_is_volume_link (const char *uri, GnomeVFSFileInfo *info)
{
	return (nautilus_link_local_get_link_type (uri, info) == NAUTILUS_LINK_MOUNT);
}

gboolean
nautilus_link_local_is_home_link (const char *uri, GnomeVFSFileInfo *info)
{
	return (nautilus_link_local_get_link_type (uri, info) == NAUTILUS_LINK_HOME);
}

gboolean
nautilus_link_local_is_trash_link (const char *uri, GnomeVFSFileInfo *info)
{
	return (nautilus_link_local_get_link_type (uri, info) == NAUTILUS_LINK_TRASH);
}

gboolean
nautilus_link_local_is_special_link (const char *uri)
{
	switch (nautilus_link_local_get_link_type (uri, NULL)) {
	case NAUTILUS_LINK_HOME:
		if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR)) {
 			return FALSE;
 		}
	case NAUTILUS_LINK_TRASH:
	case NAUTILUS_LINK_MOUNT:
		return TRUE;
	case NAUTILUS_LINK_GENERIC:
		return FALSE;
	}

	return FALSE;
}

void
nautilus_link_local_create_from_gnome_entry (GnomeDesktopItem *item,
					     const char *dest_uri,
					     const GdkPoint *position,
					     int screen)
{
	nautilus_link_desktop_file_local_create_from_gnome_entry (item, dest_uri, position, screen);
}
