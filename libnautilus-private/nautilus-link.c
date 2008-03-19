/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.c: .desktop link files.
 
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

#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-program-choosing.h"
#include "nautilus-icon-names.h"
#include <eel/eel-vfs-extensions.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define NAUTILUS_LINK_GENERIC_TAG	"Link"
#define NAUTILUS_LINK_TRASH_TAG 	"X-nautilus-trash"
#define NAUTILUS_LINK_MOUNT_TAG 	"FSDevice"
#define NAUTILUS_LINK_HOME_TAG 		"X-nautilus-home"

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
is_local_file_a_link (const char *uri)
{
	gboolean link;
	GFile *file;
	GFileInfo *info;
	GError *error;

	error = NULL;
	link = FALSE;

	file = g_file_new_for_uri (uri);

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  0, NULL, &error);
	if (info) {
		link = is_link_mime_type (g_file_info_get_content_type (info));
		g_object_unref (info);
	}
	else {
		g_warning ("Error getting info: %s\n", error->message);
		g_error_free (error);
	}

	g_object_unref (file);

	return link;
}

static gboolean
is_link_data (const char *file_contents, int file_size)
{
	char *mimetype;
	gboolean res;

	mimetype = g_content_type_guess (NULL, file_contents, file_size, NULL);
	res =  is_link_mime_type (mimetype);
	g_free (mimetype);
	return res;
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
nautilus_link_local_create (const char     *directory_uri,
			    const char     *base_name,
			    const char     *display_name,
			    const char     *image,
			    const char     *target_uri,
			    const GdkPoint *point,
			    int             screen,
			    gboolean        unique_filename)
{
	char *real_directory_uri;
	char *uri, *contents;
	GnomeDesktopItem *desktop_item;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;

	g_return_val_if_fail (directory_uri != NULL, FALSE);
	g_return_val_if_fail (base_name != NULL, FALSE);
	g_return_val_if_fail (display_name != NULL, FALSE);
	g_return_val_if_fail (target_uri != NULL, FALSE);

	if (eel_uri_is_trash (directory_uri) ||
	    eel_uri_is_search (directory_uri)) {
		return FALSE;
	}

	if (eel_uri_is_desktop (directory_uri)) {
		real_directory_uri = nautilus_get_desktop_directory_uri ();
	} else {
		real_directory_uri = g_strdup (directory_uri);
	}

	if (unique_filename) {
		uri = nautilus_ensure_unique_file_name (real_directory_uri,
							base_name, ".desktop");
		if (uri == NULL) {
			g_free (real_directory_uri);
			return FALSE;
		}
	} else {
		char *link_name;
		GFile *dir, *link;

		link_name = g_strdup_printf ("%s.desktop", base_name);

		/* replace '/' with '-', just in case */
		g_strdelimit (link_name, "/", '-');

		dir = g_file_new_for_uri (directory_uri);
		link = g_file_get_child (dir, link_name);

		uri = g_file_get_uri (link);

		g_free (link_name);
		g_object_unref (dir);
		g_object_unref (link);
	}

	g_free (real_directory_uri);

	contents = g_strdup_printf ("[Desktop Entry]\n"
				    "Encoding=UTF-8\n"
				    "Name=%s\n"
				    "Type=Link\n"
				    "URL=%s\n"
				    "%s%s\n",
				    display_name,
				    target_uri,
				    image != NULL ? "Icon=" : "",
				    image != NULL ? image : "");

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
	nautilus_directory_notify_files_added_by_uri (&dummy_list);
	nautilus_directory_schedule_metadata_remove_by_uri (&dummy_list);

	if (point != NULL) {
		item.location = g_file_new_for_uri (uri);
		item.set = TRUE;
		item.point.x = point->x;
		item.point.y = point->y;
		item.screen = screen;
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
		g_object_unref (item.location);
	}

	gnome_desktop_item_unref (desktop_item);
	g_free (contents);
	g_free (uri);
	return TRUE;
}

gboolean
nautilus_link_local_set_text (const char *uri,
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
nautilus_link_local_get_text (const char *path)
{
	return slurp_key_string (path, "Name", TRUE);
}

char *
nautilus_link_local_get_additional_text (const char *path)
{
	/* The comment field of current .desktop files is often bad.
	 * It just contains a copy of the name. This is probably because the
	 * panel shows the comment field as a tooltip.
	 */
	return NULL;
#ifdef THIS_IS_NOT_USED_RIGHT_NOW
	char *type;
	char *retval;

	if (!is_local_file_a_link (uri)) {
		return NULL;
	}

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

static char *
nautilus_link_get_link_uri_from_desktop (GnomeDesktopItem *desktop_file)
{
	const char *type;
	char *retval;

	retval = NULL;

	type = gnome_desktop_item_get_string (desktop_file, "Type");
	if (type == NULL) {
		return NULL;
	}

	if (strcmp (type, "URL") == 0) {
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
nautilus_link_get_link_name_from_desktop (GnomeDesktopItem *desktop_file)
{
	return g_strdup (gnome_desktop_item_get_localestring (desktop_file, "Name"));
}

static char *
nautilus_link_get_link_icon_from_desktop (GnomeDesktopItem *desktop_file)
{
	char *icon_uri, *icon_copy, *p;
	const char *icon;
	GnomeDesktopItemType desktop_type;

	icon_uri = g_strdup (gnome_desktop_item_get_string (desktop_file, "X-Nautilus-Icon"));
	if (icon_uri != NULL) {
		return icon_uri;
	}

	icon = gnome_desktop_item_get_string (desktop_file, GNOME_DESKTOP_ITEM_ICON);
	if (icon != NULL) {
		icon_copy = g_strdup (icon);
		if (!g_path_is_absolute (icon_copy)) {
			/* Strip out any extension on non-filename icons. Old desktop files may have this */
			p = strchr (icon_copy, '.');
                        /* Only strip known icon extensions */
			if ((p != NULL) &&
                         ((g_ascii_strcasecmp (p, ".png") == 0)
                       || (g_ascii_strcasecmp (p, ".svn") == 0)
                       || (g_ascii_strcasecmp (p, ".jpg") == 0)
                       || (g_ascii_strcasecmp (p, ".xpm") == 0)
                       || (g_ascii_strcasecmp (p, ".bmp") == 0)
                       || (g_ascii_strcasecmp (p, ".jpeg") == 0))) {
				*p = 0;
			}
		}
		return icon_copy;
	}

	desktop_type = gnome_desktop_item_get_entry_type (desktop_file);
	switch (desktop_type) {
	case GNOME_DESKTOP_ITEM_TYPE_APPLICATION:
		return g_strdup ("gnome-fs-executable");

	case GNOME_DESKTOP_ITEM_TYPE_LINK:
		return g_strdup ("gnome-dev-symlink");

	case GNOME_DESKTOP_ITEM_TYPE_FSDEVICE:
		return g_strdup ("gnome-dev-harddisk");

	case GNOME_DESKTOP_ITEM_TYPE_DIRECTORY:
		return g_strdup (NAUTILUS_ICON_FOLDER);

	case GNOME_DESKTOP_ITEM_TYPE_SERVICE:
	case GNOME_DESKTOP_ITEM_TYPE_SERVICE_TYPE:
		return g_strdup ("gnome-fs-web");

	default:
		return g_strdup ("gnome-fs-regular");
	}

	g_assert_not_reached ();
	return NULL;
}

char *
nautilus_link_local_get_link_uri (const char *uri)
{
	GnomeDesktopItem *desktop_file;
	char *retval;

	if (!is_local_file_a_link (uri)) {
		return NULL;
	}

	desktop_file = gnome_desktop_item_new_from_uri (uri, 0, NULL);
	if (desktop_file == NULL) {
		return NULL;
	}

	retval = nautilus_link_get_link_uri_from_desktop (desktop_file);
	gnome_desktop_item_unref (desktop_file);

	return retval;
}

void
nautilus_link_get_link_info_given_file_contents (const char  *file_contents,
						 int          link_file_size,
						 char       **uri,
						 char       **name,
						 char       **icon,
						 gboolean    *is_launcher)
{
	GnomeDesktopItem *desktop_file;
	const char *type;

	if (!is_link_data (file_contents, link_file_size)) {
		return;
	}

	desktop_file = gnome_desktop_item_new_from_string (NULL, file_contents, link_file_size, 0, NULL);
	if (desktop_file == NULL) {
		return; 
	}

	*uri = nautilus_link_get_link_uri_from_desktop (desktop_file);
	*name = nautilus_link_get_link_name_from_desktop (desktop_file);
	*icon = nautilus_link_get_link_icon_from_desktop (desktop_file);

	*is_launcher = FALSE;
	type = gnome_desktop_item_get_string (desktop_file, "Type");
	if (type != NULL &&
	    strcmp (type, "Application") == 0 &&
	    gnome_desktop_item_get_string (desktop_file, "Exec") != NULL) {
		*is_launcher = TRUE;
	}
	
	gnome_desktop_item_unref (desktop_file);
}

void
nautilus_link_local_create_from_gnome_entry (GnomeDesktopItem  *entry,
					     const char        *dest_uri,
					     const GdkPoint    *position,
					     int                screen)
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
	nautilus_directory_notify_files_added_by_uri (&dummy_list);
	nautilus_directory_schedule_metadata_remove_by_uri (&dummy_list);

	if (position != NULL) {
		item.location = g_file_new_for_uri (file_uri);
		item.set = TRUE;
		item.point.x = position->x;
		item.point.y = position->y;
		item.screen = screen;
		
		dummy_list.data = &item;
		dummy_list.next = NULL;
		dummy_list.prev = NULL;
	
		nautilus_directory_schedule_position_set (&dummy_list);
		g_object_unref (item.location);
	}
	gnome_desktop_item_unref (new_entry);
}
