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
#include <bonobo-activation/bonobo-activation-activate.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <stdio.h>

static int         gnome_vfs_mime_application_has_id             (GnomeVFSMimeApplication  *application,
								  const char               *id);
static int         gnome_vfs_mime_id_matches_application         (const char               *id,
								  GnomeVFSMimeApplication  *application);
static gboolean    gnome_vfs_mime_application_has_id_not_in_list (GnomeVFSMimeApplication  *application,
								  GList                    *ids);
static gboolean   application_supports_uri_scheme                (gpointer                 data,
								  gpointer                 uri_scheme);

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

static GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_file_internal (NautilusFile *file,
							 gboolean     *user_chosen)
{
	char *mime_type;
	GnomeVFSMimeApplication *result;
	char *default_application_string;
	gboolean used_user_chosen_info;
	char *uri_scheme;

	if (!nautilus_mime_actions_check_if_open_with_attributes_ready (file)) {
		return NULL;
	}

	used_user_chosen_info = TRUE;

	default_application_string = nautilus_file_get_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION, NULL);

	/* FIXME bugzilla.gnome.org 45085: should fall back to normal default 
	   if user-specified default is bogus */

	uri_scheme = nautilus_file_get_uri_scheme (file);

	result = NULL;
	if (default_application_string != NULL) {
		result = gnome_vfs_application_registry_get_mime_application (default_application_string);
		if (result != NULL && !application_supports_uri_scheme (result, uri_scheme)) {
			gnome_vfs_mime_application_free (result);
			result = NULL;
		}
	}

	if (result == NULL) {
		/* TODO: this should maybe use gnome_vfs_mime_get_default_application_for_scheme,
		   but thats not public atm */
		   
		mime_type = nautilus_file_get_mime_type (file);
		result = gnome_vfs_mime_get_default_application (mime_type);
		if (result != NULL && !application_supports_uri_scheme (result, uri_scheme)) {
			result = NULL;
		}

		if (result == NULL) {
			GList *all_applications, *l;
			
			all_applications = nautilus_mime_get_open_with_applications_for_file (file);

			for (l = all_applications; l != NULL; l = l->next) {
				result = gnome_vfs_mime_application_copy (l->data);
				if (result != NULL && !application_supports_uri_scheme (result, uri_scheme)) {
					gnome_vfs_mime_application_free (result);
					result = NULL;
				}
				if (result != NULL)
					break;
			}
			gnome_vfs_mime_application_list_free (all_applications);
		}

		g_free (mime_type);
		used_user_chosen_info = FALSE;
	} 

	g_free (uri_scheme);

	if (user_chosen != NULL) {
		*user_chosen = used_user_chosen_info;
	}

	g_free (default_application_string);

	return result;
}

GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_file (NautilusFile *file)
{
	return nautilus_mime_get_default_application_for_file_internal (file, NULL);
}

static GList *
get_open_with_mime_applications (NautilusFile *file)
{
	char *guessed_mime_type;
	char *mime_type;
	GList *result;

	guessed_mime_type = nautilus_file_get_guessed_mime_type (file);
	mime_type = nautilus_file_get_mime_type (file);

	result = gnome_vfs_mime_get_all_applications (mime_type);

	if (strcmp (guessed_mime_type, mime_type) != 0) {
		GList *result_2;
		GList *l;

		result_2 = gnome_vfs_mime_get_all_applications (guessed_mime_type);
		for (l = result_2; l != NULL; l = l->next) {
			if (!g_list_find_custom (result,
						 ((GnomeVFSMimeApplication*)l->data)->id,
						 (GCompareFunc) gnome_vfs_mime_application_has_id)) {
				result = g_list_prepend (result, l->data);
			}
		}
		g_list_free (result_2);
	}

	g_free (mime_type);
	g_free (guessed_mime_type);
	
	return result;
}

/* Get a list of applications for the Open With menu.  This is 
 * different than nautilus_mime_get_applications_for_file()
 * because this function will merge the lists of the fast and slow
 * mime types for the file */
GList *
nautilus_mime_get_open_with_applications_for_file (NautilusFile      *file)
{
	char *uri_scheme;
	GList *result;
	GList *removed;
	GList *metadata_application_add_ids;
	GList *metadata_application_remove_ids;
	GList *p;
	GnomeVFSMimeApplication *application;

	if (!nautilus_mime_actions_check_if_open_with_attributes_ready (file)) {
		return NULL;
	}

	result = get_open_with_mime_applications (file);

	/* First remove applications that cannot support this location */
	uri_scheme = nautilus_file_get_uri_scheme (file);
	g_assert (uri_scheme != NULL);
	result = eel_g_list_partition (result, application_supports_uri_scheme,
					    uri_scheme, &removed);
	gnome_vfs_mime_application_list_free (removed);
	g_free (uri_scheme);
	
	metadata_application_add_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_ADD,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);
	metadata_application_remove_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_REMOVE,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);


	result = eel_g_list_partition (result, (EelPredicateFunction) gnome_vfs_mime_application_has_id_not_in_list, 
				       metadata_application_remove_ids, &removed);
	
	gnome_vfs_mime_application_list_free (removed);

	result = g_list_reverse (result);
	for (p = metadata_application_add_ids; p != NULL; p = p->next) {
		if (g_list_find_custom (result,
					p->data,
					(GCompareFunc) gnome_vfs_mime_application_has_id) == NULL &&
		    g_list_find_custom (metadata_application_remove_ids,
					p->data,
					(GCompareFunc) strcmp) == NULL) {
			application = gnome_vfs_application_registry_get_mime_application (p->data);
			if (application != NULL) {
				result = g_list_prepend (result, application);
			}
		}
	}
	result = g_list_reverse (result);

	eel_g_list_free_deep (metadata_application_add_ids);
	eel_g_list_free_deep (metadata_application_remove_ids);

	return result;
}

GList *
nautilus_mime_get_applications_for_file (NautilusFile      *file)
{
	char *mime_type;
	GList *result;
	GList *metadata_application_ids;
	GList *p;
	GnomeVFSMimeApplication *application;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	metadata_application_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);

	mime_type = nautilus_file_get_mime_type (file);

	result = gnome_vfs_mime_get_all_applications (mime_type);

	for (p = metadata_application_ids; p != NULL; p = p->next) {
		if (!g_list_find_custom (result,
					 p->data,
					 (GCompareFunc) gnome_vfs_mime_application_has_id)) {
			application = gnome_vfs_application_registry_get_mime_application (p->data);

			if (application != NULL) {
				result = g_list_prepend (result, application);
			}
		}
	}

	eel_g_list_free_deep (metadata_application_ids);
	g_free (mime_type);
	return result;
}

static int
application_supports_uri_scheme_strcmp_style (gconstpointer application_data,
					      gconstpointer uri_scheme)
{
	return application_supports_uri_scheme
		((gpointer) application_data,
		 (gpointer) uri_scheme) ? 0 : 1;
}

gboolean
nautilus_mime_has_any_applications_for_file (NautilusFile      *file)
{
	GList *all_applications_for_mime_type, *application_that_can_access_uri;
	char *uri_scheme;
	gboolean result;

	all_applications_for_mime_type = nautilus_mime_get_applications_for_file (file);

	uri_scheme = nautilus_file_get_uri_scheme (file);
	application_that_can_access_uri = g_list_find_custom
		(all_applications_for_mime_type,
		 uri_scheme,
		 application_supports_uri_scheme_strcmp_style);
	g_free (uri_scheme);

	result = application_that_can_access_uri != NULL;
	gnome_vfs_mime_application_list_free (all_applications_for_mime_type);

	return result;
}

GnomeVFSResult
nautilus_mime_set_default_application_for_file (NautilusFile      *file,
						const char        *application_id)
{
	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	nautilus_file_set_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION, NULL, application_id);
	
	return GNOME_VFS_OK;
}

static int
gnome_vfs_mime_application_has_id (GnomeVFSMimeApplication *application, 
				   const char *id)
{
	return strcmp (application->id, id);
}

static int
gnome_vfs_mime_id_matches_application (const char *id, 
				       GnomeVFSMimeApplication *application)
{
	return gnome_vfs_mime_application_has_id (application, id);
}

static gboolean
gnome_vfs_mime_application_has_id_not_in_list (GnomeVFSMimeApplication *application, 
					       GList                   *ids)
{
	return g_list_find_custom (ids, application, 
				   (GCompareFunc) gnome_vfs_mime_id_matches_application) == NULL;
}

static gboolean
application_supports_uri_scheme (gpointer data,
				 gpointer uri_scheme)
{
	GnomeVFSMimeApplication *application;

	g_assert (data != NULL);
	application = (GnomeVFSMimeApplication *) data;

	/* The default supported uri scheme is "file" */
	if (application->supported_uri_schemes == NULL
	    && g_ascii_strcasecmp ((const char *) uri_scheme, "file") == 0) {
		return TRUE;
	}
	return g_list_find_custom (application->supported_uri_schemes,
				   uri_scheme,
				   eel_strcasecmp_compare_func) != NULL;
}
