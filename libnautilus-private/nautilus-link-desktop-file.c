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
#include "nautilus-link-desktop-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-xml-extensions.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libxml/parser.h>
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

static char *
slurp_key_string (const char *uri,
		  const char *keyname,
                  gboolean    localize)
{
	GnomeDesktopItem *desktop_file;
	const char *text;
	char *result;

	desktop_file = gnome_desktop_item_new_from_uri (uri, 0, NULL);
	if (desktop_file == NULL) {
		return NULL;
	}

        if (localize) {
                text = gnome_desktop_item_get_localestring (desktop_file, keyname);
	} else {
                text = gnome_desktop_item_get_string (desktop_file, keyname);
	}
	
	result = g_strdup (text);
	gnome_desktop_item_unref (desktop_file);

	return result;
}

gboolean
nautilus_link_desktop_file_local_create (const char        *directory_uri,
					 const char        *name,
					 const char        *image,
					 const char        *target_uri,
					 const GdkPoint    *point,
					 NautilusLinkType   type)
{
	char *uri, *contents, *temp;
	GnomeDesktopItem *desktop_item;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;

	g_return_val_if_fail (directory_uri != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (target_uri != NULL, FALSE);

	uri = g_strdup_printf ("%s/%s", directory_uri, name);

	contents = g_strdup_printf ("[Desktop Entry]\n"
				    "Encoding=UTF-8\n"
				    "Name=%s\n"
				    "Type=%s\n"
				    "URL=%s\n",
				    name,
				    get_tag (type),
				    target_uri);
				    
	if (image != NULL) {
		temp = g_strdup_printf ("%s"
					"X-Nautilus-Icon=%s\n",
					contents,
					image);
		g_free (contents);
		contents = temp;
		temp = NULL;
	}
	
	desktop_item = gnome_desktop_item_new_from_string (uri,
							   contents,
							   strlen (contents),
							   0,
							   NULL);
	if (!desktop_item) {
		g_free (contents);
		g_free (uri);
		return FALSE;
	}

	if (!gnome_desktop_item_save (desktop_item, uri, TRUE, NULL)) {
		gnome_desktop_item_unref (desktop_item);
		g_free (contents);
		g_free (uri);
		return FALSE;
	}

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

	gnome_desktop_item_unref (desktop_item);
	g_free (contents);
	g_free (uri);
	return TRUE;
}

gboolean
nautilus_link_desktop_file_local_set_icon (const char *uri,
					   const char *icon_name)
{
	GnomeDesktopItem *desktop_file;
	gboolean success;

	desktop_file = gnome_desktop_item_new_from_uri (uri, 0, NULL);
	if (desktop_file == NULL) {
		return FALSE;
	}

	gnome_desktop_item_set_string (desktop_file, "X-Nautilus-Icon", icon_name);
	success = gnome_desktop_item_save (desktop_file, NULL, FALSE, NULL);
	gnome_desktop_item_unref (desktop_file);
	
	return success;
}

gboolean
nautilus_link_desktop_file_local_set_text (const char *uri,
					   const char *text)
{
	GnomeDesktopItem *desktop_file;
	gboolean success;

	desktop_file = gnome_desktop_item_new_from_uri (uri, 0, NULL);
	if (desktop_file == NULL) {
		return FALSE;
	}

	gnome_desktop_item_set_localestring (desktop_file, "Name", text);
	success = gnome_desktop_item_save (desktop_file, NULL, FALSE, NULL);
	gnome_desktop_item_unref (desktop_file);
	
	return success;
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
	char *type;
	char *retval;

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
	char *type;
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

static char *
nautilus_link_desktop_file_get_link_uri_from_desktop (GnomeDesktopItem *desktop_file)
{
	char *terminal_command;
	const char *launch_string;
	gboolean need_term;
	const char *type;
	char *retval;

	retval = NULL;

	type = gnome_desktop_item_get_string (desktop_file, "Type");
	if (type == NULL) {
		return NULL;
	}

	if (strcmp (type, "Application") == 0) {
		launch_string = gnome_desktop_item_get_string (desktop_file, "Exec");
		if (launch_string == NULL) {
			return NULL;
		}

		need_term = gnome_desktop_item_get_boolean (desktop_file, "Terminal");
		if (need_term) {
			terminal_command = eel_gnome_make_terminal_command (launch_string);
			retval = g_strconcat ("command:", terminal_command, NULL);
			g_free (terminal_command);
		} else {
			retval = g_strconcat ("command:", launch_string, NULL);
		}
	} else if (strcmp (type, "URL") == 0) {
		/* Some old broken desktop files use this nonstandard feature, we need handle it though */
		retval = g_strdup (gnome_desktop_item_get_string (desktop_file, "Exec"));
	} else if ((strcmp (type, NAUTILUS_LINK_GENERIC_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_MOUNT_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_TRASH_TAG) == 0) ||
		   (strcmp (type, NAUTILUS_LINK_HOME_TAG) == 0)) {
		retval = g_strdup (gnome_desktop_item_get_string (desktop_file, "URL"));
	}

	return retval;
}

static char *
nautilus_link_desktop_file_get_link_name_from_desktop (GnomeDesktopItem *desktop_file)
{
	return g_strdup (gnome_desktop_item_get_localestring (desktop_file, "Name"));
}

static char *
nautilus_link_desktop_file_get_link_icon_from_desktop (GnomeDesktopItem *desktop_file)
{
	char *icon_uri;
	char *absolute;
	char *icon_name;

	icon_uri = g_strdup (gnome_desktop_item_get_string (desktop_file, "X-Nautilus-Icon"));
	if (icon_uri != NULL) {
		return icon_uri;
	}

	/* Fall back to a standard icon. */
	icon_name = g_strdup (gnome_desktop_item_get_string (desktop_file, "Icon"));
	if (icon_name == NULL) {
		return NULL;
	}
	
	if (icon_name[0] == '/') {
		absolute = icon_name;
	} else {
		absolute = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP, icon_name, TRUE, NULL);
		g_free (icon_name);
		if (absolute == NULL) {
			return NULL;
		}
	}

	icon_uri = gnome_vfs_get_uri_from_local_path (absolute);
	g_free (absolute);
	return icon_uri;
}

char *
nautilus_link_desktop_file_local_get_link_uri (const char *uri)
{
	GnomeDesktopItem *desktop_file;
	char *retval;

	desktop_file = gnome_desktop_item_new_from_uri (uri, 0, NULL);
	if (desktop_file == NULL) {
		return NULL;
	}
	
	retval = nautilus_link_desktop_file_get_link_uri_from_desktop (desktop_file);
	gnome_desktop_item_unref (desktop_file);

	return retval;
}

char *
nautilus_link_desktop_file_get_link_uri_given_file_contents (const char *uri,
							     const char *link_file_contents,
							     int         link_file_size)
{
	GnomeDesktopItem *desktop_file;
	char *retval;

	desktop_file = gnome_desktop_item_new_from_string (uri, link_file_contents, link_file_size, 0, NULL);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_desktop_file_get_link_uri_from_desktop (desktop_file);

	gnome_desktop_item_unref (desktop_file);
	return retval;
}

char *
nautilus_link_desktop_file_get_link_name_given_file_contents (const char *uri,
							      const char *link_file_contents,
							      int         link_file_size)
{
	GnomeDesktopItem *desktop_file;
	char *retval;

	desktop_file = gnome_desktop_item_new_from_string (uri, link_file_contents, link_file_size, 0, NULL);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_desktop_file_get_link_name_from_desktop (desktop_file);

	gnome_desktop_item_unref (desktop_file);
	return retval;
}


char *
nautilus_link_desktop_file_get_link_icon_given_file_contents (const char *uri,
							      const char *link_file_contents,
							      int         link_file_size)
{
	GnomeDesktopItem *desktop_file;
	char *retval;

	desktop_file = gnome_desktop_item_new_from_string (uri, link_file_contents, link_file_size, 0, NULL);
	if (desktop_file == NULL) {
		return NULL; 
	}
	retval = nautilus_link_desktop_file_get_link_icon_from_desktop (desktop_file);

	gnome_desktop_item_unref (desktop_file);
	return retval;
}


void
nautilus_link_desktop_file_local_create_from_gnome_entry (GnomeDesktopItem  *entry,
							  const char        *dest_uri,
							  const GdkPoint    *position)
{
	GList dummy_list;
	NautilusFileChangesQueuePosition item;
	GnomeDesktopItem *new_entry;
	char *file_uri;
	const char *name;

	name = gnome_desktop_item_get_string (entry, GNOME_DESKTOP_ITEM_NAME);
	file_uri = g_strdup_printf ("%s/%s.desktop", dest_uri, name);

	new_entry = gnome_desktop_item_copy (entry);
	gnome_desktop_item_save (new_entry, file_uri, TRUE, NULL);

	dummy_list.data = file_uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_added (&dummy_list);
	nautilus_directory_schedule_metadata_remove (&dummy_list);

	if (position != NULL) {
		item.uri = file_uri;
		item.set = TRUE;
		item.point.x = position->x;
		item.point.y = position->y;
		
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
	}
	gnome_desktop_item_unref (new_entry);
}
