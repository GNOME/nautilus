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
#include <stdlib.h>

#include <parser.h>
#include <xmlmemory.h>

#include <libgnomevfs/gnome-vfs.h>

#include "nautilus-link.h"
#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-preferences.h"

#define REMOTE_ICON_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL | GNOME_VFS_PERM_GROUP_ALL | GNOME_VFS_PERM_OTHER_ALL)

typedef struct NautilusLinkIconNotificationInfo NautilusLinkIconNotificationInfo;
struct NautilusLinkIconNotificationInfo
{
	char *link_uri;
	char *file_path;
};

/* given a uri, returns TRUE if it's a link file */

gboolean
nautilus_link_is_link_file(const char *file_uri)
{
	if (file_uri == NULL)
		return FALSE;
		
	return nautilus_str_has_suffix(file_uri, LINK_SUFFIX);
}

/* returns additional text to display under the name, NULL if none */
char* nautilus_link_get_additional_text(const char *link_file_uri)
{
	xmlDoc *doc;
	char *file_uri;
	char *extra_text = NULL;
	
	if (link_file_uri == NULL)
		return NULL;
	
	file_uri = gnome_vfs_unescape_string(link_file_uri, "/");
	doc = xmlParseFile (file_uri + 7);
	if (doc) {
		extra_text = xmlGetProp (doc->root, NAUTILUS_METADATA_KEY_EXTRA_TEXT);
		if (extra_text)
			extra_text = g_strdup(extra_text);
		xmlFreeDoc (doc);
	}  
	g_free(file_uri);
	return extra_text;
}

/* utility to return the local pathname of a cached icon, given the leaf name */
/* if the icons directory hasn't been created yet, create it */

static char *
make_local_path(const char *image_name)
{
	GnomeVFSResult result;
	char *escaped_uri, *local_directory_path, *local_file_path;
	
	escaped_uri = nautilus_str_escape_slashes (image_name + 7);		
	local_directory_path = g_strdup_printf("%s/.nautilus/remote_icons", g_get_home_dir());

	/* we must create the directory if it doesnt exist */
	result = gnome_vfs_make_directory(local_directory_path, REMOTE_ICON_DIR_PERMISSIONS);
		
	local_file_path = nautilus_make_path(local_directory_path, escaped_uri);
	g_free(escaped_uri);
	g_free(local_directory_path);
	return local_file_path;
}

/* utility to free the icon notification info */

static void
free_icon_notification_info (NautilusLinkIconNotificationInfo *info)
{
	g_free(info->link_uri);
	g_free(info->file_path);
	g_free(info);
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
		free_icon_notification_info(info);
		return;
	}

	/* write out the file into the cache area */	
	size = file_size;
	outfile = fopen((char*) info->file_path, "w");	 	

	fwrite(file_contents, size, 1, outfile);
	fclose(outfile);

	/* tell the world that the file has changed */
	file = nautilus_file_get(info->link_uri);
	if (file != NULL) {
		nautilus_file_changed(file);
		nautilus_file_unref(file);
	}
	
	/* free up the notification info */	
	free_icon_notification_info(info);
}

/* utility to use gnome-vfs to determine if a uri is local or not */
static gboolean
is_remote_uri (const char *uri)
{
	gboolean is_local;
	GnomeVFSURI *vfs_uri;
	
	vfs_uri = gnome_vfs_uri_new(uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref(vfs_uri);
	return !is_local;
}

/* returns the image associated with a link file */
char*
nautilus_link_get_image_uri (const char *link_file_uri)
{
	xmlDoc *doc;
	char *file_uri, *icon_uri;
	char *local_path, *local_uri;
	NautilusLinkIconNotificationInfo *info;
	
	icon_uri = NULL;
	if (link_file_uri == NULL)
		return NULL;
	
	file_uri = gnome_vfs_unescape_string(link_file_uri, "/");
	doc = xmlParseFile (file_uri + 7);
	if (doc) {
		icon_uri = xmlGetProp (doc->root, NAUTILUS_METADATA_KEY_CUSTOM_ICON);
		if (icon_uri)
			icon_uri = g_strdup(icon_uri);

		xmlFreeDoc (doc);
	}
	g_free(file_uri);
	
	/* if the image is remote, see if we can find it in our local cache */
	
	if (is_remote_uri(icon_uri)) {
	
		local_path = make_local_path(icon_uri);
		
		if (g_file_exists(local_path)) {
			g_free(icon_uri);			
			local_uri = nautilus_get_uri_from_local_path(local_path);
			g_free(local_path);
			return local_uri;	
		} 
	 
		/*load it asynchronously through gnome-vfs */
	        info = g_new0 (NautilusLinkIconNotificationInfo, 1);
		info->link_uri = g_strdup(link_file_uri);
		info->file_path = g_strdup(local_path);
		nautilus_read_entire_file_async (icon_uri, icon_read_done_callback, info);
		
		g_free(icon_uri);
  		g_free(local_path);
		return NULL; /* return NULL since the icon is still loading - it will get correctly set by the callback */
	}
			   
	return icon_uri;
}

/* returns the link uri associated with a link file */

char*		
nautilus_link_get_link_uri(const char *link_file_uri)
{
	xmlDoc *doc;
	char *file_uri;
	char* result = NULL;
	
	if (link_file_uri == NULL)
		return NULL;
		
	file_uri = gnome_vfs_unescape_string(link_file_uri, "/");
	doc = xmlParseFile (file_uri + 7);
	if (doc) {
		char* link_str = xmlGetProp (doc->root, "LINK");
		if (link_str) 
			result = g_strdup(link_str);
	
		xmlFreeDoc (doc);
	} 	
	
	if (result == NULL)
		result = g_strdup(link_file_uri);
	
	g_free(file_uri);
	return result;
}

/* strips the suffix from the passed in string if it's a link file */
char*
nautilus_link_get_display_name(char* link_file_name)
{
	if (nautilus_preferences_get_boolean(NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME, FALSE))
		return link_file_name;
		
	if (link_file_name && nautilus_str_has_suffix(link_file_name, LINK_SUFFIX)) {
		char *suffix_pos = strstr(link_file_name, LINK_SUFFIX);
		if (suffix_pos)
			*suffix_pos = '\0';
	}
	
	return link_file_name;
}
