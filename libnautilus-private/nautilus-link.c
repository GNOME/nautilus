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
#include <glib/gi18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <stdlib.h>

/* NOTE: This is pretty ugly.
 * We once supported another type of link, "historical" links, which were xml files.
 * I've now removed that code, but that makes this file sort of unnecessary, and we
 * could clean up the code a lot since we know we're dealing with desktop files.
 */

static gboolean
is_link_mime_type (const char *mime_type)
{
	if (mime_type != NULL &&
	    (g_ascii_strcasecmp (mime_type, "application/x-gnome-app-info") == 0 ||
	     g_ascii_strcasecmp (mime_type, "application/x-desktop") == 0)) {
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
is_local_file_a_link (const char *uri, GnomeVFSFileInfo *opt_info)
{
	gboolean link;
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
		link = is_link_mime_type (info->mime_type);
	} else {
		link = FALSE;
	}

	if (!opt_info && info)
		gnome_vfs_file_info_unref (info);

	return link;
}

static gboolean
is_link_data (const char *file_contents, int file_size)
{
	return is_link_mime_type
		(gnome_vfs_get_mime_type_for_data (file_contents, file_size));
}

gboolean
nautilus_link_local_create (const char *directory_uri,
			    const char *file_name,
			    const char *display_name,
			    const char *image,
			    const char *target_uri,
			    const GdkPoint *point,
			    int screen,
			    gboolean unique_filename)
{
	return nautilus_link_desktop_file_local_create (directory_uri,
							file_name,
							display_name, image,
							target_uri, 
							point, screen,
							unique_filename);
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_local_get_additional_text (const char *uri)
{
	if (!is_local_file_a_link (uri, NULL)) {
		return NULL;
	}
	
	return nautilus_link_desktop_file_local_get_additional_text (uri);
}

/* Returns the link uri associated with a link file. */
char *
nautilus_link_local_get_link_uri (const char *uri)
{
	if (!is_local_file_a_link (uri, NULL)) {
		return NULL;
	}
	return nautilus_link_desktop_file_local_get_link_uri (uri);
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
	
	if (is_link_data (file_contents, link_file_size)) {
		nautilus_link_desktop_file_get_link_info_given_file_contents (file_contents, link_file_size, uri, name, icon, drive_id, volume_id);
	}
}

void
nautilus_link_local_create_from_gnome_entry (GnomeDesktopItem *item,
					     const char *dest_uri,
					     const GdkPoint *position,
					     int screen)
{
	nautilus_link_desktop_file_local_create_from_gnome_entry (item, dest_uri, position, screen);
}
