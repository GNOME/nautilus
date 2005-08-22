/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-actions.c - uri-specific versions of mime action functions

   Copyright (C) 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>
#include "nautilus-mime-actions.h"
 
#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <string.h>

static GList*
filter_nautilus_handler (GList *apps)
{
	GList *l, *next;
	GnomeVFSMimeApplication *application;

	l = apps;
	while (l != NULL) {
		application = (GnomeVFSMimeApplication *) l->data;
		next = l->next;

		if (strcmp (gnome_vfs_mime_application_get_desktop_id (application),
			   "nautilus-folder-handler.desktop") == 0) {
			gnome_vfs_mime_application_free (application);
			apps = g_list_delete_link (apps, l); 
		}

		l = next;
	}

	return apps;
}

static gboolean
nautilus_mime_actions_check_if_minimum_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_minimum_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

NautilusFileAttributes 
nautilus_mime_actions_get_minimum_file_attributes (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_VOLUMES |
		NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI |
		NAUTILUS_FILE_ATTRIBUTE_METADATA |
		NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE |
		NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE;
}

static NautilusFileAttributes 
nautilus_mime_actions_get_open_with_file_attributes (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_VOLUMES |
		NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI |
		NAUTILUS_FILE_ATTRIBUTE_METADATA |
		NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE;
}

static gboolean
nautilus_mime_actions_check_if_open_with_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_open_with_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

NautilusFileAttributes 
nautilus_mime_actions_get_full_file_attributes (void)
{
	return nautilus_mime_actions_get_minimum_file_attributes () |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES;
}

GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_file (NautilusFile *file)
{
	GnomeVFSMimeApplication *app;
	char *uri, *mime_type;

	if (!nautilus_mime_actions_check_if_open_with_attributes_ready (file)) {
		return NULL;
	}
	
	uri = nautilus_file_get_uri (file);
	mime_type = nautilus_file_get_mime_type (file);
	app = gnome_vfs_mime_get_default_application_for_uri (uri, mime_type);
	
	g_free (uri);
	g_free (mime_type);
	
	return app;
}

static int
application_equal (GnomeVFSMimeApplication *app_a, GnomeVFSMimeApplication *app_b)
{
	return gnome_vfs_mime_application_equal (app_a, app_b) ? 0 : 1;
}

static GList *
get_open_with_mime_applications (NautilusFile *file)
{
	char *guessed_mime_type;
	char *mime_type, *uri;
	GList *result;

	guessed_mime_type = nautilus_file_get_guessed_mime_type (file);
	mime_type = nautilus_file_get_mime_type (file);
	uri = nautilus_file_get_uri (file);

	result = gnome_vfs_mime_get_all_applications_for_uri (uri, mime_type);

	if (strcmp (guessed_mime_type, mime_type) != 0) {
		GList *result_2;
		GList *l;

		result_2 = gnome_vfs_mime_get_all_applications (guessed_mime_type);
		for (l = result_2; l != NULL; l = l->next) {
			if (!g_list_find_custom (result, l->data,
						 (GCompareFunc) application_equal)) {
				result = g_list_prepend (result, l->data);
			}
		}
		g_list_free (result_2);
	}

	g_free (mime_type);
	g_free (uri);
	g_free (guessed_mime_type);
	
	return g_list_reverse (result);
}

/* Get a list of applications for the Open With menu.  This is 
 * different than nautilus_mime_get_applications_for_file()
 * because this function will merge the lists of the fast and slow
 * mime types for the file */
GList *
nautilus_mime_get_open_with_applications_for_file (NautilusFile *file)
{
	GList *result;

	if (!nautilus_mime_actions_check_if_open_with_attributes_ready (file)) {
		return NULL;
	}

	result = get_open_with_mime_applications (file);

	return filter_nautilus_handler (result);
}

GList *
nautilus_mime_get_applications_for_file (NautilusFile *file)
{
	char *mime_type;
	GList *result;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}
	mime_type = nautilus_file_get_mime_type (file);
	result = gnome_vfs_mime_get_all_applications (mime_type);

	return filter_nautilus_handler (result);
}

gboolean
nautilus_mime_has_any_applications_for_file (NautilusFile *file)
{
	GList *apps;
	char *uri, *mime_type;
	gboolean result;

	uri = nautilus_file_get_uri (file);
	mime_type = nautilus_file_get_mime_type (file);
	
	apps = gnome_vfs_mime_get_all_applications_for_uri (uri, mime_type);
	apps = filter_nautilus_handler (apps);
		
	if (apps) {
		result = TRUE;
		gnome_vfs_mime_application_list_free (apps);
	} else {
		result = FALSE;
	}
	
	g_free (mime_type);
	g_free (uri);

	return result;
}
