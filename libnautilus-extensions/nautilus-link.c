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
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdlib.h>

/* Link type XML tags */
#define NAUTILUS_LINK_GENERIC_TAG	"Generic Link"
#define NAUTILUS_LINK_TRASH_TAG 	"Trash Link"
#define NAUTILUS_LINK_MOUNT_TAG 	"Mount Link"
#define NAUTILUS_LINK_HOME_TAG 		"Home Link"

#define REMOTE_ICON_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
				     | GNOME_VFS_PERM_GROUP_ALL \
				     | GNOME_VFS_PERM_OTHER_ALL)

typedef struct {
	char *link_uri;
	char *file_path;
} NautilusLinkIconNotificationInfo;

typedef void (* NautilusFileFunction) (NautilusFile *file);

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

static NautilusLinkType
get_link_type (const char *tag)
{
	if (tag != NULL) {
		if (strcmp (tag, NAUTILUS_LINK_TRASH_TAG) == 0) {
			return NAUTILUS_LINK_TRASH;
		}
		if (strcmp (tag, NAUTILUS_LINK_MOUNT_TAG) == 0) {
			return NAUTILUS_LINK_MOUNT;
		}
		if (strcmp (tag, NAUTILUS_LINK_HOME_TAG) == 0) {
			return NAUTILUS_LINK_HOME;
		}
	}
	return NAUTILUS_LINK_GENERIC;
}

gboolean
nautilus_link_local_create (const char *directory_path,
			    const char *name,
			    const char *image,
			    const char *target_uri,
			    const GdkPoint *point,
			    NautilusLinkType type)
{
	xmlDocPtr output_document;
	xmlNodePtr root_node;
	char *path;
	int result;
	char *uri;
	GList dummy_list;
	NautilusFileChangesQueuePosition item;

	
	g_return_val_if_fail (directory_path != NULL, FALSE);
	g_return_val_if_fail (name != NULL, FALSE);
	g_return_val_if_fail (image != NULL, FALSE);
	g_return_val_if_fail (target_uri != NULL, FALSE);
	
	/* create a new xml document */
	output_document = xmlNewDoc ("1.0");
	
	/* add the root node to the output document */
	root_node = xmlNewDocNode (output_document, NULL, "nautilus_object", NULL);
	xmlDocSetRootElement (output_document, root_node);

	/* Add mime magic string so that the mime sniffer can recognize us.
	 * Note: The value of the tag identfies what type of link this.  */
	xmlSetProp (root_node, "nautilus_link", get_tag (type));
	
	/* Add link and custom icon tags */
	xmlSetProp (root_node, "custom_icon", image);
	xmlSetProp (root_node, "link", target_uri);
	
	/* all done, so save the xml document as a link file */
	path = nautilus_make_path (directory_path, name);
	result = xmlSaveFile (path, output_document);
	
	xmlFreeDoc (output_document);

	if (result <= 0) {
		g_free (path);
		return FALSE;
	}
	
	/* Notify that this new file has been created. */
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

static char *
xml_get_root_property (xmlDoc *doc,
		       const char *key)
{
	char *property, *duplicate;
	
	/* Need to g_strdup so we can free with g_free instead of xmlFree. */
	property = xmlGetProp (xmlDocGetRootElement (doc), key);
	duplicate = g_strdup (property);
	xmlFree (property);
	return duplicate;
}

static char *
local_get_root_property (const char *path,
			 const char *key)
{
	xmlDoc *document;
	char *property;
	const char *mime_type;
	
	property = NULL;

	/* Check mime type. Exit if it is not a nautilus link */
	mime_type = gnome_vfs_get_file_mime_type (path, NULL, FALSE);
	if (strcmp (mime_type, "application/x-nautilus-link") != 0) {
		return NULL;
	}
	
	document = xmlParseFile (path);
	if (document != NULL) {
		property = xml_get_root_property (document, key);
		xmlFreeDoc (document);
	}
	
	return property;
}

static gboolean
local_set_root_property (const char *path,
			 const char *key,
			 const char *value,
			 NautilusFileFunction extra_notify)
{
	xmlDocPtr document;
	xmlNodePtr root;
	const char *old_value;
	char *uri;
	NautilusFile *file;

	document = xmlParseFile (path);
	if (document == NULL) {
		return FALSE;
	}
	root = xmlDocGetRootElement (document);
	if (root == NULL) {
		xmlFreeDoc (document);
		return FALSE;
	}

	/* Check if the property value is already correct. */
	old_value = xmlGetProp (root, key);
	if (old_value != NULL && strcmp (old_value, value) == 0) {
		xmlFreeDoc (document);
		return TRUE;
	}

	/* Change and write the property. */
	xmlSetProp (root, key, value);
	xmlSaveFile (path, document);
	xmlFreeDoc (document);

	/* Notify about the change. */
	uri = gnome_vfs_get_uri_from_local_path (path);
	file = nautilus_file_get (uri);
	if (file != NULL) {
		if (extra_notify != NULL) {
			(* extra_notify) (file);
		}
		nautilus_file_changed (file);
		nautilus_file_unref (file);
	}
	g_free (uri);
		
	return TRUE;
}

/* Set the icon for a link file. This can only be called on local
 * paths, and only on files known to be link files.
 */
gboolean
nautilus_link_local_set_icon (const char *path, const char *icon_name)
{
	return local_set_root_property (path,
					NAUTILUS_METADATA_KEY_CUSTOM_ICON,
					icon_name,
					NULL);
}


static void
forget_file_activation_uri (NautilusFile *file)
{
	GList *attributes;

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	nautilus_file_invalidate_attributes (file, attributes);
	g_list_free (attributes);
}

/* Set the link uri for a link file. This can only be called on local
 * paths, and only on files known to be link files.
 */
gboolean
nautilus_link_local_set_link_uri (const char *path, const char *link_uri)
{
	return local_set_root_property (path,
					"link",
					link_uri,
					forget_file_activation_uri);
}

gboolean
nautilus_link_local_set_type (const char *path,
			      NautilusLinkType type)
{
	return local_set_root_property (path,
					"nautilus_link",
					get_tag (type),
					NULL);
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_local_get_additional_text (const char *path)
{
	return local_get_root_property
		(path, NAUTILUS_METADATA_KEY_EXTRA_TEXT);
}

/* utility to return the local pathname of a cached icon, given the leaf name */
/* if the icons directory hasn't been created yet, create it */
static char *
make_local_path (const char *image_uri)
{
	GnomeVFSResult result;
	
	char *escaped_uri, *local_directory_path, *local_directory_uri, *local_file_path;
	
	escaped_uri = gnome_vfs_escape_slashes (image_uri);		
	
	local_directory_path = g_strconcat
		(g_get_home_dir (),
		 "/.nautilus/remote_icons",
		 NULL);

	/* We must create the directory if it doesn't exist. */
	local_directory_uri = gnome_vfs_get_uri_from_local_path (local_directory_path);
	result = gnome_vfs_make_directory (local_directory_uri, REMOTE_ICON_DIR_PERMISSIONS);
	if (result != GNOME_VFS_OK) {
		g_free (local_directory_uri);
		g_free (escaped_uri);
		g_free (local_directory_path);
		return NULL;
	}
			
	local_file_path = nautilus_make_path (local_directory_path, escaped_uri);
	g_free (local_directory_uri);
	g_free (escaped_uri);
	g_free (local_directory_path);

	return local_file_path;
}

/* utility to free the icon notification info */

static void
free_icon_notification_info (NautilusLinkIconNotificationInfo *info)
{
	g_free (info->link_uri);
	g_free (info->file_path);
	g_free (info);
}

/* callback to handle the asynchronous reading of icons */
static void
icon_read_done_callback (GnomeVFSResult result,
			 GnomeVFSFileSize file_size,
			 char *file_contents,
			 gpointer callback_data)
{
	int size;
	FILE* outfile;
	NautilusFile *file;
	NautilusLinkIconNotificationInfo *info;
	
	info = (NautilusLinkIconNotificationInfo *) callback_data;

	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		free_icon_notification_info (info);
		return;
	}

	/* write out the file into the cache area */	
	size = file_size;
	outfile = fopen (info->file_path, "wb");	 	
	fwrite (file_contents, size, 1, outfile);
	fclose (outfile);

	g_free (file_contents);

	/* tell the world that the file has changed */
	file = nautilus_file_get (info->link_uri);
	if (file != NULL) {
		nautilus_file_changed (file);
		nautilus_file_unref (file);
	}
	
	/* free up the notification info */	
	free_icon_notification_info (info);
}

/* returns the image associated with a link file */
char *
nautilus_link_local_get_image_uri (const char *path)
{
	xmlDoc *doc;
	char *icon_uri;
	char *local_path, *local_uri;
	NautilusLinkIconNotificationInfo *info;
	
	doc = xmlParseFile (path);
	if (doc == NULL) {
		return NULL;
	}
	
	icon_uri = xml_get_root_property (doc, NAUTILUS_METADATA_KEY_CUSTOM_ICON);
	xmlFreeDoc (doc);

	if (icon_uri == NULL) {
		return NULL;
	}
	
	/* if the image is remote, see if we can find it in our local cache */
	if (eel_is_remote_uri (icon_uri)) {
		local_path = make_local_path (icon_uri);
		if (local_path == NULL) {
			g_free (icon_uri);
			return NULL;
		}
		if (g_file_exists (local_path)) {
			g_free (icon_uri);			
			local_uri = gnome_vfs_get_uri_from_local_path (local_path);
			g_free (local_path);
			return local_uri;	
		}
	 
		/* load it asynchronously through gnome-vfs */
	        info = g_new0 (NautilusLinkIconNotificationInfo, 1);
		info->link_uri = gnome_vfs_get_uri_from_local_path (path);
		info->file_path = g_strdup (local_path);
		eel_read_entire_file_async (icon_uri, icon_read_done_callback, info);
		
		g_free (icon_uri);
  		g_free (local_path);
		return NULL; /* return NULL since the icon is still loading - it will get correctly set by the callback */
	}
	
	return icon_uri;
}

/* Returns the link uri associated with a link file. */
char *
nautilus_link_local_get_link_uri (const char *path)
{
	return local_get_root_property (path, "link");
}

/* Returns the link type of the link file. */
NautilusLinkType
nautilus_link_local_get_link_type (const char *path)
{
	return get_link_type (local_get_root_property (path, "nautilus_link"));
}

/* FIXME bugzilla.eazel.com 2495: 
 * Caller has to know to pass in a file with a NUL character at the end.
 */
char *
nautilus_link_get_link_uri_given_file_contents (const char *file_contents,
						int file_size)
{
	xmlDoc *doc;
	char *property;
	
	doc = xmlParseMemory ((char *) file_contents, file_size);
	property = xml_get_root_property (doc, "link");
	xmlFreeDoc (doc);
	return property;
}

gboolean
nautilus_link_local_is_volume_link (const char *path)
{
	return nautilus_link_local_get_link_type (path) == NAUTILUS_LINK_MOUNT;
}

gboolean
nautilus_link_local_is_home_link (const char *path)
{
	return nautilus_link_local_get_link_type (path) == NAUTILUS_LINK_HOME;
}

gboolean
nautilus_link_local_is_trash_link (const char *path)
{
	return nautilus_link_local_get_link_type (path) == NAUTILUS_LINK_TRASH;
}


void
nautilus_link_local_create_from_gnome_entry (GnomeDesktopEntry *entry, const char *dest_path, const GdkPoint *position)
{
	char *icon_name;
	char *launch_string, *terminal_command;
	char *quoted, *arguments, *temp_str;
	int i;

	if (entry == NULL || dest_path == NULL) {
		return;
	}
	
	/* Extract arguments from exec array */
	arguments = NULL;
	for (i = 0; i < entry->exec_length; ++i) {
		quoted = eel_shell_quote (entry->exec[i]);
		if (arguments == NULL) {
			arguments = quoted;
		} else {
			temp_str = arguments;
			arguments = g_strconcat (arguments, " ", quoted, NULL);
			g_free (temp_str);
			g_free (quoted);
		}
	}
		
	if (strcmp (entry->type, "Application") == 0) {
		if (entry->terminal) {
			terminal_command = eel_gnome_make_terminal_command (arguments);
			launch_string = g_strconcat ("command:", terminal_command, NULL);
			g_free (terminal_command);
		} else {
			launch_string = g_strconcat ("command:", arguments, NULL);
		}		
	} else if (strcmp (entry->type, "URL") == 0) {
		launch_string = g_strdup (arguments);
	} else {
		/* Unknown .desktop file type */
		launch_string = NULL;
	}
	
	if (entry->icon != NULL) {
		icon_name = eel_make_uri_from_half_baked_uri (entry->icon);
	} else {
		icon_name = g_strdup ("gnome-unknown.png");
	}
	
	if (launch_string != NULL) {
		nautilus_link_local_create (dest_path, entry->name, icon_name, 
			    	    	    launch_string, position, NAUTILUS_LINK_GENERIC);
	}
	
	g_free (icon_name);
	g_free (launch_string);
	g_free (arguments);
}
