/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link-historical.c: xml-based link files.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-link.h"
#include "nautilus-link-historical.h"

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
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <stdlib.h>


#define NAUTILUS_LINK_GENERIC_TAG	"Generic Link"
#define NAUTILUS_LINK_TRASH_TAG 	"Trash Link"
#define NAUTILUS_LINK_MOUNT_TAG 	"Mount Link"
#define NAUTILUS_LINK_HOME_TAG 		"Home Link"

#define REMOTE_ICON_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
				     | GNOME_VFS_PERM_GROUP_ALL \
				     | GNOME_VFS_PERM_OTHER_ALL)

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
nautilus_link_historical_local_create (const char       *directory_path,
				       const char       *name,
				       const char       *image,
				       const char       *target_uri,
				       const GdkPoint   *point,
				       NautilusLinkType  type)
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
local_set_root_property (const char *uri,
			 const char *key,
			 const char *value,
			 NautilusFileFunction extra_notify)
{
	xmlDocPtr document;
	xmlNodePtr root;
	xmlChar *old_value;
	char *path;
	NautilusFile *file;

	path = gnome_vfs_get_local_path_from_uri (uri);
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
		xmlFree (old_value);
		return TRUE;
	}

	xmlFree (old_value);

	/* Change and write the property. */
	xmlSetProp (root, key, value);
	xmlSaveFile (path, document);
	xmlFreeDoc (document);

	/* Notify about the change. */
	file = nautilus_file_get (uri);
	if (file != NULL) {
		if (extra_notify != NULL) {
			(* extra_notify) (file);
		}
		nautilus_file_changed (file);
		nautilus_file_unref (file);
	}
	g_free (path);
		
	return TRUE;
}


/* Set the icon for a link file. This can only be called on local
 * paths, and only on files known to be link files.
 */
gboolean
nautilus_link_historical_local_set_icon (const char *path, const char *icon_name)
{
	return local_set_root_property (path,
					NAUTILUS_METADATA_KEY_CUSTOM_ICON,
					icon_name,
					NULL);
}


/* Set the link uri for a link file. This can only be called on local
 * paths, and only on files known to be link files.
 */
gboolean
nautilus_link_historical_local_set_link_uri (const char *path, const char *link_uri)
{
	return local_set_root_property (path,
					"link",
					link_uri,
					NULL);
}

gboolean
nautilus_link_historical_local_set_type (const char *path,
				   NautilusLinkType type)
{
	return local_set_root_property (path,
					"nautilus_link",
					get_tag (type),
					NULL);
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_historical_local_get_additional_text (const char *path)
{
	return local_get_root_property
		(path, NAUTILUS_METADATA_KEY_EXTRA_TEXT);
}


/* Returns the link uri associated with a link file. */
char *
nautilus_link_historical_local_get_link_uri (const char *path)
{
	return local_get_root_property (path, "link");
}

/* Returns the link type of the link file. */
NautilusLinkType
nautilus_link_historical_local_get_link_type (const char *path)
{
	char *property;
	NautilusLinkType type;
	
	property = local_get_root_property (path, "nautilus_link");
	type = get_link_type (property);
	g_free (property);

	return type;
}

/* FIXME bugzilla.eazel.com 2495: 
 * Caller has to know to pass in a file with a NUL character at the end.
 */
char *
nautilus_link_historical_get_link_uri_given_file_contents (const char *file_contents,
							   int file_size)
{
	xmlDoc *doc;
	char *property;
	
	doc = xmlParseMemory ((char *) file_contents, file_size);
	property = xml_get_root_property (doc, "link");
	xmlFreeDoc (doc);
	return property;
}


char *
nautilus_link_historical_get_link_icon_given_file_contents (const char *file_contents,
							    int         file_size)
{
	xmlDoc *doc;
	char *property;
	
	doc = xmlParseMemory ((char *) file_contents, file_size);
	property = xml_get_root_property (doc, NAUTILUS_METADATA_KEY_CUSTOM_ICON);
	xmlFreeDoc (doc);
	return property;
}


gboolean
nautilus_link_historical_local_is_volume_link (const char *path)
{
	return nautilus_link_historical_local_get_link_type (path) == NAUTILUS_LINK_MOUNT;
}

gboolean
nautilus_link_historical_local_is_home_link (const char *path)
{
	return nautilus_link_historical_local_get_link_type (path) == NAUTILUS_LINK_HOME;
}

gboolean
nautilus_link_historical_local_is_trash_link (const char *path)
{
	return nautilus_link_historical_local_get_link_type (path) == NAUTILUS_LINK_TRASH;
}


void
nautilus_link_historical_local_create_from_gnome_entry (GnomeDesktopEntry *entry, const char *dest_path, const GdkPoint *position)
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
		nautilus_link_historical_local_create (dest_path, entry->name, icon_name, 
						       launch_string, position, NAUTILUS_LINK_GENERIC);
	}
	
	g_free (icon_name);
	g_free (launch_string);
	g_free (arguments);
}


