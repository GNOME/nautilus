/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.c: xml-based link files.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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

/* FIXME: Bad to include the private file here, but necessary for now. */
#include "nautilus-directory-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-preferences.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <parser.h>
#include <stdlib.h>
#include <xmlmemory.h>

#define REMOTE_ICON_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
				     | GNOME_VFS_PERM_GROUP_ALL \
				     | GNOME_VFS_PERM_OTHER_ALL)

typedef struct {
	char *link_uri;
	char *file_path;
} NautilusLinkIconNotificationInfo;

gboolean
nautilus_link_create (const char *directory_path,
		      const char *name,
		      const char *image,
		      const char *target_uri)
{
	xmlDocPtr output_document;
	xmlNodePtr root_node;
	char *path;
	int result;
	char *uri;
	GList dummy_list;
	
	/* create a new xml document */
	output_document = xmlNewDoc ("1.0");
	
	/* add the root node to the output document */
	root_node = xmlNewDocNode (output_document, NULL, "NAUTILUS_OBJECT", NULL);
	xmlDocSetRootElement (output_document, root_node);

	/* Add mime magic string so that the mime sniffer can recognize us.
	 * Note: The value of the tag has no meaning.  */
	xmlSetProp (root_node, "NAUTILUS_LINK", "Nautilus Link");

	/* Add link and custom icon tags */
	xmlSetProp (root_node, "CUSTOM_ICON", image);
	xmlSetProp (root_node, "LINK", target_uri);
	
	/* all done, so save the xml document as a link file */
	path = nautilus_make_path (directory_path, name);
	result = xmlSaveFile (path, output_document);
	
	xmlFreeDoc (output_document);

	if (result <= 0) {
		g_free (path);
		return FALSE;
	}
	
	/* Notify that this new file has been created. */
	uri = nautilus_get_uri_from_local_path (path);
	dummy_list.data = uri;
	dummy_list.next = NULL;
	dummy_list.prev = NULL;
	nautilus_directory_notify_files_added (&dummy_list);
	g_free (uri);

	g_free (path);

	return TRUE;
}

/* Given a NautilusFile, returns TRUE if it's known to be a link file. */
gboolean
nautilus_link_is_link_file (NautilusFile *file)
{
	return nautilus_file_is_mime_type (file, "application/x-nautilus-link");
}

/* Set the icon for a link file. This can only be called on local
 * paths, and only on files known to be link files.
 */
gboolean
nautilus_link_set_icon (const char *path, const char *icon_name)
{
	xmlDocPtr document;
	char *uri;
	NautilusFile *file;

	document = xmlParseFile (path);
	if (document == NULL) {
		return FALSE;
	}

	xmlSetProp (xmlDocGetRootElement (document),
		    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
		    icon_name);
	xmlSaveFile (path, document);
	xmlFreeDoc (document);

	uri = nautilus_get_uri_from_local_path (path);
	file = nautilus_file_get (uri);
	if (file != NULL) {
		nautilus_file_changed (file);
		nautilus_file_unref (file);		
	}
	g_free (uri);
		
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
nautilus_link_get_root_property (const char *link_file_uri,
				 const char *key)
{
	char *path, *property;
	xmlDoc *doc;
	
	if (link_file_uri == NULL) {
		return NULL;
	}
	
	/* FIXME: Works only with local link files. */
	path = nautilus_get_local_path_from_uri (link_file_uri);
	if (path == NULL) {
		return NULL;
	}

	/* FIXME: Sync. I/O. */
	doc = xmlParseFile (path);
	g_free (path);
	property = xml_get_root_property (doc, key);
	xmlFreeDoc (doc);
	return property;
}

/* returns additional text to display under the name, NULL if none */
char *
nautilus_link_get_additional_text (const char *link_file_uri)
{
	/* FIXME: This interface requires sync. I/O. */
	return nautilus_link_get_root_property
		(link_file_uri, NAUTILUS_METADATA_KEY_EXTRA_TEXT);
}

/* utility to return the local pathname of a cached icon, given the leaf name */
/* if the icons directory hasn't been created yet, create it */
static char *
make_local_path (const char *image_uri)
{
	char *escaped_uri, *unescaped_uri, *local_directory_path, *local_file_path;
	
	/* we can't call nautilus_get_local_path_from_uri here, since it will return NULL because
	   it's not a local uri, but we still should unescape */
	unescaped_uri = gnome_vfs_unescape_string (image_uri, "/");
	escaped_uri = nautilus_str_escape_slashes (unescaped_uri + 7);		
	g_free (unescaped_uri);
	
	local_directory_path = g_strconcat
		(g_get_home_dir (),
		 "/.nautilus/remote_icons",
		 NULL);

	/* We must create the directory if it doesn't exist. */
	/* FIXME: Is it OK to ignore the error here? */
	gnome_vfs_make_directory (local_directory_path, REMOTE_ICON_DIR_PERMISSIONS);

	local_file_path = nautilus_make_path (local_directory_path, escaped_uri);
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
nautilus_link_get_image_uri (const char *link_file_uri)
{
	xmlDoc *doc;
	char *path, *icon_uri;
	char *local_path, *local_uri;
	NautilusLinkIconNotificationInfo *info;
	
	if (link_file_uri == NULL) {
		return NULL;
	}
	
	/* FIXME: Works only with local URIs. */
	path = nautilus_get_local_path_from_uri (link_file_uri);
	if (path == NULL) {
		return NULL;
	}

	doc = xmlParseFile (path);
	g_free (path);
	icon_uri = xml_get_root_property (doc, NAUTILUS_METADATA_KEY_CUSTOM_ICON);
	xmlFreeDoc (doc);

	/* if the image is remote, see if we can find it in our local cache */
	if (nautilus_is_remote_uri (icon_uri)) {
		local_path = make_local_path (icon_uri);
		if (g_file_exists (local_path)) {
			g_free (icon_uri);			
			local_uri = nautilus_get_uri_from_local_path (local_path);
			g_free (local_path);
			return local_uri;	
		}
	 
		/* load it asynchronously through gnome-vfs */
	        info = g_new0 (NautilusLinkIconNotificationInfo, 1);
		info->link_uri = g_strdup (link_file_uri);
		info->file_path = g_strdup (local_path);
		nautilus_read_entire_file_async (icon_uri, icon_read_done_callback, info);
		
		g_free (icon_uri);
  		g_free (local_path);
		return NULL; /* return NULL since the icon is still loading - it will get correctly set by the callback */
	}
			   
	return icon_uri;
}

/* Returns the link uri associated with a link file. */
char *
nautilus_link_get_link_uri (const char *link_file_uri)
{
	/* FIXME: This interface requires sync. I/O. */
	return nautilus_link_get_root_property
		(link_file_uri, "LINK");
}

/* FIXME: Caller has to know to pass in a file with a NUL character at
 * the end.
 */
char *
nautilus_link_get_link_uri_given_file_contents (const char *file_contents,
						int file_size)
{
	xmlDoc *doc;
	char *property;
	
	doc = xmlParseMemory ((char *) file_contents, file_size);
	property = xml_get_root_property (doc, "LINK");
	xmlFreeDoc (doc);
	return property;
}
