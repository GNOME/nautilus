/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link-desktop-file.c: .desktop link files.
 
   Copyright (C) 2001 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the historicalied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Jonathan Blandford <jrb@redhat.com>
            Alexander Larsson <alexl@redhat.com>
*/
#include <config.h>
#include "nautilus-link.h"
#include "nautilus-link-desktop-file.h"
#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include "nautilus-file-utilities.h"
#include "nautilus-desktop-file-loader.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdlib.h>

#define NAUTILUS_LINK_GENERIC_TAG	"Link"
#define NAUTILUS_LINK_TRASH_TAG 	"X-nautilus-trash"
#define NAUTILUS_LINK_MOUNT_TAG 	"FSDevice"
#define NAUTILUS_LINK_HOME_TAG 		"X-nautilus-home"

static const char *
get_tag (NautilusLinkType type)
{
	switch (type) {
	default:
		g_assert_not_reached ();
		/* fall through */
	case NAUTILUS_LINK_GENERIC:
		return NAUTILUS_LINK_GENERIC_TAG;
	case NAUTILUS_LINK_TRASH:
		return NAUTILUS_LINK_TRASH_TAG;
	case NAUTILUS_LINK_MOUNT:
		return NAUTILUS_LINK_MOUNT_TAG;
	case NAUTILUS_LINK_HOME:
		return NAUTILUS_LINK_HOME_TAG;
	}
}

static gchar *
slurp_key_string (const char *path,
		  const char *keyname,
                  gboolean    localize)
{
	NautilusDesktopFile *desktop_file = NULL;
	gchar *text;
	gboolean set;
	GnomeVFSResult result;
	gchar *uri;

	uri = gnome_vfs_get_uri_from_local_path (path);
	if (uri == NULL) {
		return NULL;
	}

	result = nautilus_desktop_file_load (uri, &desktop_file);
	
	g_free (uri);

	if (result != GNOME_VFS_OK) {
		return NULL;
	}

        if (localize) {
                set = nautilus_desktop_file_get_locale_string (desktop_file,
							       "Desktop Entry",
							       keyname,
							       &text);
	} else {
                set = nautilus_desktop_file_get_string (desktop_file,
							"Desktop Entry",
							keyname,
							&text);
	}
	
	nautilus_desktop_file_free (desktop_file);
	
	if (set == FALSE) {
		return NULL;
	}

	return text;
}

gboolean
nautilus_link_desktop_file_local_create (const char        *directory_path,
					 const char        *name,
					 const char        *image,
					 const char        *target_uri,
					 const GdkPoint    *point,
					 NautilusLinkType   type)
{
	gchar *path;
	FILE *file;
	char *uri;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;
	GnomeVFSHandle *handle;

	g_return_val_if_fail (directory_path != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (image != NULL, FALSE);
	g_return_val_if_fail (target_uri != NULL, FALSE);

	path = nautilus_make_path (directory_path, name);
	handle = NULL;

	file = fopen (path, "w");

	if (file == NULL) {
		g_free (path);
		return FALSE;
	}

	fputs ("[Desktop Entry]\nEncoding=Legacy-Mixed\nName=", file);
	fputs (name, file);
	fputs ("\nType=", file);
	fputs (get_tag (type), file);
	fputs ("\nX-Nautilus-Icon=", file);
	fputs (image, file);
	fputs ("\nURL=", file);
	fputs (target_uri, file);
	fputs ("\n", file);
	/* ... */
	fclose (file);

	uri = gnome_vfs_get_uri_from_local_path (path);
	dummy_list.data = uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_added (&dummy_list);
	nautilus_directory_schedule_metadata_remove (&dummy_list);

	if (point != NULL) {
		item.uri = uri;
		item.set = TRUE;
		item.point.x = point->x;
		item.point.y = point->y;
		
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
	}

	g_free (uri);
	g_free (path);
	return TRUE;
}

gboolean
nautilus_link_desktop_file_local_set_icon (const char *path,
					   const char *icon_name)
{
	NautilusDesktopFile *desktop_file;
	GnomeVFSResult result;
	char *uri;

	uri = gnome_vfs_get_uri_from_local_path (path);
	if (uri == NULL) {
		return FALSE;
	}
	
	result = nautilus_desktop_file_load (uri, &desktop_file);

	if (result != GNOME_VFS_OK) {
		g_free (uri);
		return FALSE;
	}

	nautilus_desktop_file_set_string (desktop_file, "Desktop Entry", "X-Nautilus-Icon", icon_name);
	
	result = nautilus_desktop_file_save (desktop_file, uri);
	nautilus_desktop_file_free (desktop_file);
	
	g_free (uri);
	
	if (result != GNOME_VFS_OK) {
		return FALSE;
	}

	return TRUE;
}

char *
nautilus_link_desktop_file_local_get_text (const char *path)
{
	return slurp_key_string (path, "Name", TRUE);
}

char *
nautilus_link_desktop_file_local_get_additional_text (const char *path)
{
	/* The comment field of current .desktop files is often bad.
	 * It just contains a copy of the name. This is probably because the
	 * panel shows the comment field as a tooltip.
	 */
	return NULL;
#ifdef THIS_IS_NOT_USED_RIGHT_NOW
	gchar *type;
	gchar *retval;

	type = slurp_key_string (path, "Type", FALSE);
	retval = NULL;
	if (type == NULL) {
		return NULL;
	}

	if (strcmp (type, "Application") == 0) {
		retval = slurp_key_string (path, "Comment", TRUE);
	}
	
	g_free (type);

	return retval;
#endif
}

NautilusLinkType
nautilus_link_desktop_file_local_get_link_type (const char *path)
{
	gchar *type;
	NautilusLinkType retval;

	type = slurp_key_string (path, "Type", FALSE);

	if (type == NULL) {
		return NAUTILUS_LINK_GENERIC;
	}
	if (strcmp (type, NAUTILUS_LINK_HOME_TAG) == 0) {
		retval = NAUTILUS_LINK_HOME;
	} else if (strcmp (type, NAUTILUS_LINK_MOUNT_TAG) == 0) {
		retval = NAUTILUS_LINK_MOUNT;
	} else if (strcmp (type, NAUTILUS_LINK_TRASH_TAG) == 0) {
		retval = NAUTILUS_LINK_TRASH;
	} else {
		retval = NAUTILUS_LINK_GENERIC;
	}

	g_free (type);
	return retval;
}

gboolean
nautilus_link_desktop_file_local_is_volume_link (const char *path)
{
	return (nautilus_link_desktop_file_local_get_link_type (path) ==  NAUTILUS_LINK_MOUNT);
}

gboolean
nautilus_link_desktop_file_local_is_home_link (const char *path)
{
	return (nautilus_link_desktop_file_local_get_link_type (path) ==  NAUTILUS_LINK_HOME);
}

gboolean
nautilus_link_desktop_file_local_is_trash_link (const char *path)
{
	return (nautilus_link_desktop_file_local_get_link_type (path) ==  NAUTILUS_LINK_TRASH);
}

static gchar *
nautilus_link_desktop_file_get_link_uri_from_desktop (NautilusDesktopFile *desktop_file)
{
	gchar *terminal_command;
	gchar *launch_string;
	gboolean need_term;
	gchar *type;
	gchar *retval;

	retval = NULL;

	type = NULL;
	if (! nautilus_desktop_file_get_string (desktop_file,
						"Desktop Entry",
						"Type",
						&type)) {
		return NULL;
	}

	if (strcmp (type, "Application") == 0) {
		if (! nautilus_desktop_file_get_string (desktop_file,
							"Desktop Entry",
							"Exec",
							&launch_string)) {
			return NULL;
		}

		need_term = FALSE;
		nautilus_desktop_file_get_boolean (desktop_file,
					      "Desktop Entry",
					      "Terminal",
					      &need_term);
		if (need_term) {
			terminal_command = eel_gnome_make_terminal_command (launch_string);
			retval = g_strconcat ("command:", terminal_command, NULL);
			g_free (terminal_command);
		} else {
			retval = g_strconcat ("command:", launch_string, NULL);
		}
		g_free (launch_string);
	} else if (strcmp (type, "URL") == 0) {
		/* Some old broken desktop files use this nonstandard feature, we need handle it though */
		nautilus_desktop_file_get_string (desktop_file,
						  "Desktop Entry",
						  "Exec",
						  &retval);
	} else if ((strcmp (type, NAUTILUS_LINK_GENERIC_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_MOUNT_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_TRASH_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_HOME_TAG) == 0)) {
		nautilus_desktop_file_get_string (desktop_file,
						  "Desktop Entry",
						  "URL",
						  &retval);
	}
	return retval;
}

static gchar *
nautilus_link_desktop_file_get_link_name_from_desktop (NautilusDesktopFile *desktop_file)
{
	gchar *name;

	name = NULL;

	if (nautilus_desktop_file_get_locale_string (desktop_file,
						     "Desktop Entry",
						     "Name",
						     &name)) {
		return name;
	} else {
		return NULL;
	}
}

static gchar *
nautilus_link_desktop_file_get_link_icon_from_desktop (NautilusDesktopFile *desktop_file)
{
	char *icon_uri;
	gchar *absolute;
	gchar *icon_name;

	if (nautilus_desktop_file_get_string (desktop_file, "Desktop Entry", "X-Nautilus-Icon", &icon_uri)) {
		return icon_uri;
	}

	/* Fall back to a standard icon. */
	if (nautilus_desktop_file_get_string (desktop_file, "Desktop Entry", "Icon", &icon_name)) {
		if (icon_name == NULL) {
			return NULL;
		}
	
		absolute = gnome_pixmap_file (icon_name);
		if (absolute != NULL) {
			g_free (icon_name);
			icon_name = absolute;
		}
		if (icon_name[0] == '/') {
			icon_uri = gnome_vfs_get_uri_from_local_path (icon_name);
		} else {
			icon_uri = NULL;
		}
		g_free (icon_name);

		return icon_uri;
	}

	return NULL;
}

char *
nautilus_link_desktop_file_local_get_link_uri (const char *path)
{
	NautilusDesktopFile *desktop_file = NULL;
	gchar *retval;
	GnomeVFSResult result;
	char *uri;

	uri = gnome_vfs_get_uri_from_local_path (path);
	if (uri == NULL) {
		return FALSE;
	}

	result = nautilus_desktop_file_load (uri, &desktop_file);
	
	g_free (uri);

	if (result != GNOME_VFS_OK) {
		return NULL;
	}
	
	retval = nautilus_link_desktop_file_get_link_uri_from_desktop (desktop_file);
	
	nautilus_desktop_file_free (desktop_file);
	return retval;
}

char *
nautilus_link_desktop_file_get_link_uri_given_file_contents (const char *link_file_contents,
							     int         link_file_size)
{
	NautilusDesktopFile *desktop_file;
	gchar *slurp;
	gchar *retval;

	slurp = g_strndup (link_file_contents, link_file_size);
	desktop_file = nautilus_desktop_file_from_string (slurp);
	g_free (slurp);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_desktop_file_get_link_uri_from_desktop (desktop_file);

	nautilus_desktop_file_free (desktop_file);
	return retval;
}

char *
nautilus_link_desktop_file_get_link_name_given_file_contents (const char *link_file_contents,
							      int         link_file_size)
{
	NautilusDesktopFile *desktop_file;
	gchar *slurp;
	gchar *retval;

	slurp = g_strndup (link_file_contents, link_file_size);
	desktop_file = nautilus_desktop_file_from_string (slurp);
	g_free (slurp);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_desktop_file_get_link_name_from_desktop (desktop_file);

	nautilus_desktop_file_free (desktop_file);
	return retval;
}


char *
nautilus_link_desktop_file_get_link_icon_given_file_contents (const char *link_file_contents,
							      int         link_file_size)
{
	NautilusDesktopFile *desktop_file;
	gchar *slurp;
	gchar *retval;

	slurp = g_strndup (link_file_contents, link_file_size);
	desktop_file = nautilus_desktop_file_from_string (slurp);
	g_free (slurp);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_desktop_file_get_link_icon_from_desktop (desktop_file);

	nautilus_desktop_file_free (desktop_file);
	return retval;
}


void
nautilus_link_desktop_file_local_create_from_gnome_entry (GnomeDesktopEntry *entry,
							  const char        *dest_path,
							  const GdkPoint    *position)
{
	char *uri;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;
	GnomeDesktopEntry *new_entry;
	char *file_name;

	new_entry = gnome_desktop_entry_copy (entry);
	g_free (new_entry->location);
	file_name = g_strdup_printf ("%s.desktop", entry->name);
	new_entry->location = nautilus_make_path (dest_path, file_name);
	g_free (file_name);
	gnome_desktop_entry_save (new_entry);

	uri = gnome_vfs_get_uri_from_local_path (dest_path);
	dummy_list.data = uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_added (&dummy_list);
	nautilus_directory_schedule_metadata_remove (&dummy_list);

	if (position != NULL) {
		item.uri = uri;
		item.set = TRUE;
		item.point.x = position->x;
		item.point.y = position->y;
		
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
	}
	gnome_desktop_entry_free (new_entry);
}

