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
#include <eel/eel-glib-extensions.h>
#include "nautilus-metadata.h"
#include <eel/eel-string.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs.h>
#include <stdio.h>
 
static int         gnome_vfs_mime_application_has_id             (GnomeVFSMimeApplication  *application,
								  const char               *id);
static int         gnome_vfs_mime_id_matches_application         (const char               *id,
								  GnomeVFSMimeApplication  *application);
static gboolean    gnome_vfs_mime_application_has_id_not_in_list (GnomeVFSMimeApplication  *application,
								  GList                    *ids);
static gboolean    string_not_in_list                            (const char               *str,
								  GList                    *list);
static char       *mime_type_get_supertype                       (const char               *mime_type);
static GList      *get_explicit_content_view_iids_from_metafile  (NautilusFile             *file);
static gboolean    server_has_content_requirements               (OAF_ServerInfo           *server);
static gboolean   application_supports_uri_scheme                (gpointer                 data,
								  gpointer                 uri_scheme);
static GList      *nautilus_do_component_query                   (const char               *mime_type,
								  const char               *uri_scheme,
								  GList                    *content_mime_types,
								  gboolean                  ignore_content_mime_types,
								  GList                    *explicit_iids,
								  char                    **extra_sort_criteria,
								  char                     *extra_requirements);
static GList      *str_list_difference                           (GList                    *a,
								  GList                    *b);
static char      **strv_concat                                   (char                    **a,
								  char                    **b);

static gboolean
is_known_mime_type (const char *mime_type)
{
	if (mime_type == NULL) {
		return FALSE;
	}
	
	if (g_strcasecmp (mime_type, GNOME_VFS_MIME_TYPE_UNKNOWN) == 0) {
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
nautilus_mime_actions_check_if_minimum_attributes_ready (NautilusFile *file)
{
	GList *attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_minimum_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);
	g_list_free (attributes);

	return ready;
}

static gboolean
nautilus_mime_actions_check_if_full_attributes_ready (NautilusFile *file)
{
	GList *attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_full_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);
	g_list_free (attributes);

	return ready;
}


GList *
nautilus_mime_actions_get_minimum_file_attributes (void)
{
	GList *attributes;

	attributes = NULL;
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_METADATA);
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE);

	return attributes;
}


GList *
nautilus_mime_actions_get_full_file_attributes (void)
{
	GList *attributes;

	attributes = nautilus_mime_actions_get_minimum_file_attributes ();
	attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES);

	return attributes;
}



GnomeVFSMimeActionType
nautilus_mime_get_default_action_type_for_file (NautilusFile *file)
{
	char *mime_type;
	char *action_type_string;
	GnomeVFSMimeActionType action_type;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return GNOME_VFS_MIME_ACTION_TYPE_NONE;
	}

	action_type_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_DEFAULT_ACTION_TYPE, NULL);

	if (action_type_string == NULL) {
		mime_type = nautilus_file_get_mime_type (file);
		action_type = gnome_vfs_mime_get_default_action_type (mime_type);
		g_free (mime_type);
		return action_type;
	} else {
		if (g_strcasecmp (action_type_string, "application") == 0) {
			return GNOME_VFS_MIME_ACTION_TYPE_APPLICATION;
		} else if (g_strcasecmp (action_type_string, "component") == 0) {
			return GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;
		} else {
			return GNOME_VFS_MIME_ACTION_TYPE_NONE;
		}
	}
}

GnomeVFSMimeAction *
nautilus_mime_get_default_action_for_file (NautilusFile *file)
{
	GnomeVFSMimeAction *action;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	action = g_new0 (GnomeVFSMimeAction, 1);

	action->action_type = nautilus_mime_get_default_action_type_for_file (file);

	switch (action->action_type) {
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		action->action.application = 
			nautilus_mime_get_default_application_for_file (file);
		if (action->action.application == NULL) {
			g_free (action);
			action = NULL;
		}
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
		action->action.component = 
			nautilus_mime_get_default_component_for_file (file);
		if (action->action.component == NULL) {
			g_free (action);
			action = NULL;
		}
	case GNOME_VFS_MIME_ACTION_TYPE_NONE:
		g_free (action);
		action = NULL;
		break;
	default:
		g_assert_not_reached ();
	}

	return action;
}


static GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_file_internal (NautilusFile *file,
							 gboolean     *user_chosen)
{
	char *mime_type;
	GnomeVFSMimeApplication *result;
	char *default_application_string;
	gboolean used_user_chosen_info;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	used_user_chosen_info = TRUE;

	default_application_string = nautilus_file_get_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION, NULL);

	/* FIXME bugzilla.gnome.org 45085: should fall back to normal default 
	   if user-specified default is bogus */

	if (default_application_string == NULL) {
		mime_type = nautilus_file_get_mime_type (file);
		result = gnome_vfs_mime_get_default_application (mime_type);
		g_free (mime_type);
		used_user_chosen_info = FALSE;
	} else {
		result = gnome_vfs_application_registry_get_mime_application (default_application_string);
	}

	if (user_chosen != NULL) {
		*user_chosen = used_user_chosen_info;
	}

	return result;
}

GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_file (NautilusFile *file)
{
	return nautilus_mime_get_default_application_for_file_internal (file, NULL);
}

gboolean
nautilus_mime_is_default_application_for_file_user_chosen (NautilusFile *file)
{
	GnomeVFSMimeApplication *application;
	gboolean user_chosen;

	application = nautilus_mime_get_default_application_for_file_internal (file, &user_chosen);

	/* Doesn't count as user chosen if the user-specified data is bogus and doesn't
	 * result in an actual application.
	 */
	if (application == NULL) {
		return FALSE;
	}

	gnome_vfs_mime_application_free (application);
	 
	return user_chosen;
}


static char **
nautilus_mime_get_default_component_sort_conditions (NautilusFile *file, char *default_component_string)
{
	char **sort_conditions;
	char *supertype;
	char *mime_type;
	GList *short_list;
	GList *p;
	char *prev;
	
	sort_conditions = g_new0 (char *, 5);
	
	mime_type = nautilus_file_get_mime_type (file);

	supertype = mime_type_get_supertype (mime_type);

	/* prefer the exact right IID */
	if (default_component_string != NULL) {
		sort_conditions[0] = g_strconcat ("iid == '", default_component_string, "'", NULL);
	} else {
		sort_conditions[0] = g_strdup ("true");
	}

	/* Prefer something from the short list */

	short_list = nautilus_mime_get_short_list_components_for_file (file);
	if (short_list != NULL) {
		sort_conditions[1] = g_strdup ("prefer_by_list_order (iid, ['");

		for (p = short_list; p != NULL; p = p->next) {
			prev = sort_conditions[1];
			
			if (p->next != NULL) {
				sort_conditions[1] = g_strconcat (prev, ((OAF_ServerInfo *) (p->data))->iid, 
								  "','", NULL);
			} else {
				sort_conditions[1] = g_strconcat (prev, ((OAF_ServerInfo *) (p->data))->iid, 
								  "'])", NULL);
			}
			g_free (prev);
		}
	} else {
		sort_conditions[1] = g_strdup ("true");
	}
	
	gnome_vfs_mime_component_list_free (short_list);

	/* Prefer something that matches the exact type to something
	   that matches the supertype */
	if (is_known_mime_type (mime_type)) {
		sort_conditions[2] = g_strconcat ("bonobo:supported_mime_types.has ('",mime_type,"')", NULL);
	} else {
		sort_conditions[2] = g_strdup ("true");
	}

	/* Prefer something that matches the supertype to something that matches `*' */
	if (is_known_mime_type (mime_type) && supertype != NULL) {
		sort_conditions[3] = g_strconcat ("bonobo:supported_mime_types.has ('",supertype,"')", NULL);
	} else {
		sort_conditions[3] = g_strdup ("true");
	}

	sort_conditions[4] = NULL;

	g_free (mime_type);
	g_free (supertype);

	return sort_conditions;
}	

static OAF_ServerInfo *
nautilus_mime_get_default_component_for_file_internal (NautilusFile *file,
						       gboolean     *user_chosen)
{
	GList *info_list;
	OAF_ServerInfo *mime_default; 
	char *default_component_string;
	char *mime_type;
	char *uri_scheme;
	GList *item_mime_types;
	GList *explicit_iids;
	OAF_ServerInfo *server;
	char **sort_conditions;
	char *extra_requirements;
	gboolean used_user_chosen_info;
	gboolean metadata_default;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	used_user_chosen_info = TRUE;

	info_list = NULL;

	mime_type = nautilus_file_get_mime_type (file);

	uri_scheme = nautilus_file_get_uri_scheme (file);

	explicit_iids = get_explicit_content_view_iids_from_metafile (file); 

	if (!nautilus_mime_actions_check_if_full_attributes_ready (file) || 
	    !nautilus_file_get_directory_item_mime_types (file, &item_mime_types)) {
		item_mime_types = NULL;
	}

	default_component_string = nautilus_file_get_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT, NULL);

    	if (default_component_string == NULL) {
		metadata_default = FALSE;

		if (is_known_mime_type (mime_type)) {
			mime_default = gnome_vfs_mime_get_default_component (mime_type);
			if (mime_default != NULL) {
				default_component_string = g_strdup (mime_default->iid);
				if (default_component_string != NULL) {
					/* Default component chosen based only on type. */
					used_user_chosen_info = FALSE;
				}
				CORBA_free (mime_default);
			}
		}
	} else {
		metadata_default = TRUE;
	}
	
	sort_conditions = nautilus_mime_get_default_component_sort_conditions (file, default_component_string);

	/* If the default is specified in the per-uri metadata,
           respect the setting regardless of content type requirements */
	if (metadata_default) {
		extra_requirements = g_strconcat ("iid == '", default_component_string, "'", NULL);
		info_list = nautilus_do_component_query (mime_type, uri_scheme, item_mime_types, TRUE,
							 explicit_iids, sort_conditions, extra_requirements);
		g_free (extra_requirements);
	}

	if (info_list == NULL) {
		info_list = nautilus_do_component_query (mime_type, uri_scheme, item_mime_types, FALSE, 
							 explicit_iids, sort_conditions, NULL);
	}

	if (info_list != NULL) {
		server = OAF_ServerInfo_duplicate (info_list->data);
		gnome_vfs_mime_component_list_free (info_list);

		if (default_component_string != NULL && strcmp (server->iid, default_component_string) == 0) {
			used_user_chosen_info = TRUE;	/* Default component chosen based on user-stored . */
		}
	} else {
		server = NULL;		
	}
	
	eel_g_list_free_deep (item_mime_types);
	g_strfreev (sort_conditions);

	g_free (uri_scheme);
	g_free (mime_type);
	g_free (default_component_string);

	if (user_chosen != NULL) {
		*user_chosen = used_user_chosen_info;
	}

	return server;
}


OAF_ServerInfo *
nautilus_mime_get_default_component_for_file (NautilusFile      *file)
{
	return nautilus_mime_get_default_component_for_file_internal (file, NULL);
}

gboolean
nautilus_mime_is_default_component_for_file_user_chosen (NautilusFile      *file)
{
	OAF_ServerInfo *component;
	gboolean user_chosen;

	component = nautilus_mime_get_default_component_for_file_internal (file, &user_chosen);

	/* Doesn't count as user chosen if the user-specified data is bogus and doesn't
	 * result in an actual component.
	 */
	if (component == NULL) {
		return FALSE;
	}

	CORBA_free (component);
	 
	return user_chosen;
}


GList *
nautilus_mime_get_short_list_applications_for_file (NautilusFile      *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *result;
	GList *removed;
	GList *metadata_application_add_ids;
	GList *metadata_application_remove_ids;
	GList *p;
	GnomeVFSMimeApplication *application;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	mime_type = nautilus_file_get_mime_type (file);
	result = gnome_vfs_mime_get_short_list_applications (mime_type);
	g_free (mime_type);

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

	return result;
}

static char *
build_joined_string (GList *list, const char *prefix, const char *separator, const char *suffix)
{
	GString *string;
	GList *node;
	char *result;

	string = g_string_new (prefix);
	if (list != NULL) {
		g_string_append (string, list->data);
		for (node = list->next; node != NULL; node = node->next) {
			g_string_append (string, separator);
			g_string_append (string, node->data);
		}
	}
	g_string_append (string, suffix);

	result = string->str;
	g_string_free (string, FALSE);
	return result;
}

GList *
nautilus_mime_get_short_list_components_for_file (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *item_mime_types;
	GList *servers;
	GList *iids;
	GList *result;
	GList *removed;
	GList *metadata_component_add_ids;
	GList *metadata_component_remove_ids;
	GList *p;
	OAF_ServerInfo *component;
	GList *explicit_iids;
	char *extra_sort_conditions[2];
	char *extra_requirements;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	uri_scheme = nautilus_file_get_uri_scheme (file);

	explicit_iids = get_explicit_content_view_iids_from_metafile (file); 

	if (!nautilus_mime_actions_check_if_full_attributes_ready (file) || 
	    !nautilus_file_get_directory_item_mime_types (file, &item_mime_types)) {
		item_mime_types = NULL;
	}

	metadata_component_add_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_ADD,
		 NAUTILUS_METADATA_SUBKEY_COMPONENT_IID);
	metadata_component_remove_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_REMOVE,
		 NAUTILUS_METADATA_SUBKEY_COMPONENT_IID);

	mime_type = nautilus_file_get_mime_type (file);
	servers = gnome_vfs_mime_get_short_list_components (mime_type);
	iids = NULL;

	for (p = servers; p != NULL; p = p->next) {
		component = (OAF_ServerInfo *) p->data;

		iids = g_list_prepend (iids, component->iid);
	}

	iids = eel_g_list_partition
		(iids, (EelPredicateFunction) string_not_in_list, 
		 metadata_component_remove_ids, &removed);

	g_list_free (removed);

	for (p = metadata_component_add_ids; p != NULL; p = p->next) {
		if (g_list_find_custom (iids,
					p->data,
					(GCompareFunc) strcmp) == NULL &&
		    g_list_find_custom (metadata_component_remove_ids,
					p->data,
					(GCompareFunc) strcmp) == NULL) {
			iids = g_list_prepend (iids, p->data);
		}
	}

	/* By copying the iids using g_list_prepend, we've reversed
	 * the short list order. We need to use the order to determine
	 * the first available component, so reverse it now to
	 * maintain original ordering.
	 */

	if (iids == NULL) {
		result = NULL;
	} else {
		iids = g_list_reverse (iids);
		extra_sort_conditions[0] = build_joined_string (iids, "prefer_by_list_order (iid, ['", "','", "'])");
		extra_sort_conditions[1] = NULL;
		extra_requirements = build_joined_string (iids, "has (['", "','", "'], iid)");
		result = nautilus_do_component_query (mime_type, uri_scheme, item_mime_types, FALSE,
						      explicit_iids, extra_sort_conditions, extra_requirements);
		g_free (extra_requirements);
		g_free (extra_sort_conditions[0]);
	}

	eel_g_list_free_deep (item_mime_types);
	gnome_vfs_mime_component_list_free (servers);
	g_list_free (iids);
	g_free (uri_scheme);
	g_free (mime_type);
	
	return result;
}

GList *
nautilus_mime_get_all_applications_for_file (NautilusFile      *file)
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

	all_applications_for_mime_type = nautilus_mime_get_all_applications_for_file (file);

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

gboolean
nautilus_mime_has_any_applications_for_file_type (NautilusFile      *file)
{
	GList *applications;
	gboolean result;

	applications = nautilus_mime_get_all_applications_for_file (file);
	
	result = applications != NULL;
	gnome_vfs_mime_application_list_free (applications);

	return result;
}

gboolean
nautilus_mime_actions_file_needs_full_file_attributes (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *info_list;
	GList *explicit_iids;
	GList *p;
	gboolean needs_full_attributes;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      FALSE);

	if (!nautilus_file_is_directory (file)) {
		return FALSE;
	}

	uri_scheme = nautilus_file_get_uri_scheme (file);

	mime_type = nautilus_file_get_mime_type (file);

	explicit_iids = get_explicit_content_view_iids_from_metafile (file); 

	info_list = nautilus_do_component_query (mime_type, uri_scheme, NULL, TRUE,
						 explicit_iids, NULL, NULL);
	
	needs_full_attributes = FALSE;

	for (p = info_list; p != NULL; p = p->next) {
		needs_full_attributes |= server_has_content_requirements ((OAF_ServerInfo *) (p->data));
	}
	
	gnome_vfs_mime_component_list_free (info_list);
	eel_g_list_free_deep (explicit_iids);
	g_free (uri_scheme);
	g_free (mime_type);

	return needs_full_attributes;
}


GList *
nautilus_mime_get_all_components_for_file (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *item_mime_types;
	GList *info_list;
	GList *explicit_iids;

	if (!nautilus_mime_actions_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	uri_scheme = nautilus_file_get_uri_scheme (file);

	mime_type = nautilus_file_get_mime_type (file);
	explicit_iids = get_explicit_content_view_iids_from_metafile (file); 

	if (!nautilus_mime_actions_check_if_full_attributes_ready (file) || 
	    !nautilus_file_get_directory_item_mime_types (file, &item_mime_types)) {
		item_mime_types = NULL;
	}

	info_list = nautilus_do_component_query (mime_type, uri_scheme, item_mime_types, FALSE,
						 explicit_iids, NULL, NULL);
	
	eel_g_list_free_deep (explicit_iids);
	eel_g_list_free_deep (item_mime_types);

	g_free (uri_scheme);
	g_free (mime_type);

	return info_list;
}

gboolean
nautilus_mime_has_any_components_for_file (NautilusFile      *file)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_all_components_for_file (file);
	result = list != NULL;
	gnome_vfs_mime_component_list_free (list);

	return result;
}


static GList *
mime_get_all_components_for_uri_scheme (const char *uri_scheme)
{
	g_return_val_if_fail (eel_strlen (uri_scheme) > 0, NULL);

	return nautilus_do_component_query
		(NULL, uri_scheme, NULL, TRUE,
		 NULL, NULL, NULL);
}

gboolean
nautilus_mime_has_any_components_for_uri_scheme (const char *uri_scheme)
{
	GList *list;
	gboolean result;

	g_return_val_if_fail (eel_strlen (uri_scheme) > 0, FALSE);

	list = mime_get_all_components_for_uri_scheme (uri_scheme);
	result = list != NULL;
	gnome_vfs_mime_component_list_free (list);

	return result;
}

GnomeVFSResult
nautilus_mime_set_default_action_type_for_file (NautilusFile           *file,
						GnomeVFSMimeActionType  action_type)
{
	const char *action_string;

	switch (action_type) {
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		action_string = "application";
		break;		
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
		action_string = "component";
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_NONE:
	default:
		action_string = "none";
	}

	nautilus_file_set_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_ACTION_TYPE, NULL, action_string);

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_default_application_for_file (NautilusFile      *file,
						const char        *application_id)
{
	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	nautilus_file_set_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION, NULL, application_id);

	/* If there's no default action type, set it to match this. */
	if (application_id != NULL && 
	    nautilus_mime_get_default_action_type_for_file (file) == GNOME_VFS_MIME_ACTION_TYPE_NONE) {
		return nautilus_mime_set_default_action_type_for_file (file, GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
	}

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_default_component_for_file (NautilusFile      *file,
					      const char        *component_iid)
{
	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	nautilus_file_set_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT, NULL, component_iid);

	/* If there's no default action type, set it to match this. */
	if (component_iid != NULL && 
	    nautilus_mime_get_default_action_type_for_file (file) == GNOME_VFS_MIME_ACTION_TYPE_NONE) {
		return nautilus_mime_set_default_action_type_for_file (file, GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
	}

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_short_list_applications_for_file (NautilusFile      *file,
						    GList             *applications)
{
	GList *add_list;
	GList *remove_list;
	GList *normal_short_list;
	GList *normal_short_list_ids;
	GList *p;
	char *mime_type;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	/* get per-mime short list */

	mime_type = nautilus_file_get_mime_type (file);
	normal_short_list = gnome_vfs_mime_get_short_list_applications (mime_type);
	g_free (mime_type);

	normal_short_list_ids = NULL;
	for (p = normal_short_list; p != NULL; p = p->next) {
		normal_short_list_ids = g_list_prepend (normal_short_list_ids, ((GnomeVFSMimeApplication *) p->data)->id);
	}

	/* compute delta */

	add_list = str_list_difference (applications, normal_short_list_ids);
	remove_list = str_list_difference (normal_short_list_ids, applications);

	nautilus_file_set_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_ADD,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID,
		 add_list);
	nautilus_file_set_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_REMOVE,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID,
		 remove_list);

	/* FIXME bugzilla.gnome.org 41269: 
	 * need to free normal_short_list, normal_short_list_ids, add_list, remove_list 
	 */

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_short_list_components_for_file (NautilusFile      *file,
						  GList             *components)
{
	GList *add_list;
	GList *remove_list;
	GList *normal_short_list;
	GList *normal_short_list_ids;
	GList *p;
	char *mime_type;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	/* get per-mime short list */
	mime_type = nautilus_file_get_mime_type (file);
	normal_short_list = gnome_vfs_mime_get_short_list_components (mime_type);
	g_free (mime_type);
	
	normal_short_list_ids = NULL;
	for (p = normal_short_list; p != NULL; p = p->next) {
		normal_short_list_ids = g_list_prepend (normal_short_list_ids, ((OAF_ServerInfo *) p->data)->iid);
	}

	/* compute delta */

	add_list = str_list_difference (components, normal_short_list_ids);
	remove_list = str_list_difference (normal_short_list_ids, components);

	nautilus_file_set_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_ADD,
		 NAUTILUS_METADATA_SUBKEY_COMPONENT_IID,
		 add_list);
	nautilus_file_set_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_REMOVE,
		 NAUTILUS_METADATA_SUBKEY_COMPONENT_IID,
		 remove_list);

	/* FIXME bugzilla.gnome.org 41269: 
	 * need to free normal_short_list, normal_short_list_ids, add_list, remove_list 
	 */

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_add_application_to_short_list_for_file (NautilusFile      *file,
						      const char        *application_id)
{
	GList *old_list, *new_list;
	GnomeVFSResult result;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	result = GNOME_VFS_OK;

	old_list = nautilus_mime_get_short_list_applications_for_file (file);

	if (!gnome_vfs_mime_id_in_application_list (application_id, old_list)) {
		new_list = g_list_append (gnome_vfs_mime_id_list_from_application_list (old_list), 
					  g_strdup (application_id));
		result = nautilus_mime_set_short_list_applications_for_file (file, new_list);
		eel_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_application_list_free (old_list);

	return result;
}

GnomeVFSResult
nautilus_mime_remove_application_from_short_list_for_file (NautilusFile      *file,
							   const char        *application_id)
{
	GList *old_list, *new_list;
	gboolean was_in_list;
	GnomeVFSResult result;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	old_list = nautilus_mime_get_short_list_applications_for_file (file);
	old_list = gnome_vfs_mime_remove_application_from_list
		(old_list, application_id, &was_in_list);

	if (!was_in_list) {
		result = GNOME_VFS_OK;
	} else {
		new_list = gnome_vfs_mime_id_list_from_application_list (old_list);
		result = nautilus_mime_set_short_list_applications_for_file (file, new_list);
		eel_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_application_list_free (old_list);
	
	return result;
}

GnomeVFSResult
nautilus_mime_add_component_to_short_list_for_file (NautilusFile      *file,
						    const char        *iid)
{
	GList *old_list, *new_list;
	GnomeVFSResult result;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	old_list = nautilus_mime_get_short_list_components_for_file (file);

	if (gnome_vfs_mime_id_in_component_list (iid, old_list)) {
		result = GNOME_VFS_OK;
	} else {
		new_list = g_list_append (gnome_vfs_mime_id_list_from_component_list (old_list), 
					  g_strdup (iid));
		result = nautilus_mime_set_short_list_components_for_file (file, new_list);
		eel_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_component_list_free (old_list);

	return result;
}

GnomeVFSResult
nautilus_mime_remove_component_from_short_list_for_file (NautilusFile *file,
							 const char   *iid)
{
	GList *old_list, *new_list;
	gboolean was_in_list;
	GnomeVFSResult result;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	old_list = nautilus_mime_get_short_list_components_for_file (file);
	old_list = gnome_vfs_mime_remove_component_from_list 
		(old_list, iid, &was_in_list);

	if (!was_in_list) {
		result = GNOME_VFS_OK;
	} else {
		new_list = gnome_vfs_mime_id_list_from_component_list (old_list);
		result = nautilus_mime_set_short_list_components_for_file (file, new_list);
		eel_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_component_list_free (old_list);

	return result;
}

GnomeVFSResult
nautilus_mime_extend_all_applications_for_file (NautilusFile *file,
						GList        *applications)
{
	GList *metadata_application_ids;
	GList *extras;
	GList *final_applications;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	metadata_application_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);

	extras = str_list_difference (applications, metadata_application_ids);

	final_applications = g_list_concat (g_list_copy (metadata_application_ids), extras);

	nautilus_file_set_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID,
		 final_applications);

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_remove_from_all_applications_for_file (NautilusFile *file,
						     GList        *applications)
{
	GList *metadata_application_ids;
	GList *final_applications;

	g_return_val_if_fail (nautilus_mime_actions_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	metadata_application_ids = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);
	
	final_applications = str_list_difference (metadata_application_ids, applications);
	
	nautilus_file_set_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID,
		 final_applications);
	
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
string_not_in_list (const char *str, 
		    GList      *list)
{
	return g_list_find_custom (list, (gpointer) str, (GCompareFunc) strcmp) == NULL;
}

static char *
extract_prefix_add_suffix (const char *string, 
			   const char *separator, 
			   const char *suffix)
{
        const char *separator_position;
        int prefix_length;
        char *result;

        separator_position = strstr (string, separator);
        prefix_length = separator_position == NULL
                ? (int) strlen (string)
                : separator_position - string;

        result = g_malloc (prefix_length + strlen(suffix) + 1);
        
        strncpy (result, string, prefix_length);
        result[prefix_length] = '\0';

        strcat (result, suffix);

        return result;
}

static char *
mime_type_get_supertype (const char *mime_type)
{
	if (mime_type == NULL || mime_type == '\0') {
		return g_strdup (mime_type);
	}
        return extract_prefix_add_suffix (mime_type, "/", "/*");
}


/*
 * The following routine uses metadata associated with the current url
 * to add content view components specified in the metadata. The
 * content views are specified in the string as <EXPLICIT_CONTENT_VIEW
 * IID="iid"/> elements inside the appropriate <FILE> element.  
 */

static GList *
get_explicit_content_view_iids_from_metafile (NautilusFile *file)
{
        if (file != NULL) {
                return nautilus_file_get_metadata_list 
                        (file,
			 NAUTILUS_METADATA_KEY_EXPLICIT_COMPONENT,
			 NAUTILUS_METADATA_SUBKEY_COMPONENT_IID);
        } else {
		return NULL;
	}
}

static char *
make_oaf_query_for_explicit_content_view_iids (GList *view_iids)
{
        GList *p;
        char  *iid;
        char  *query;
        char  *old_query;

        query = NULL;

        for (p = view_iids; p != NULL; p = p->next) {
                iid = (char *) p->data;
                if (query != NULL) {
                        old_query = query;
                        query = g_strconcat (query, " OR ", NULL);
                        g_free (old_query);
                } else {
                        query = g_strdup ("(");
                }

                old_query = query;
                query = g_strdup_printf ("%s iid=='%s'", old_query, iid);
                g_free (old_query);
        }


        if (query != NULL) {
                old_query = query;
                query = g_strconcat (old_query, ")", NULL);
                g_free (old_query);
        } else {
                query = g_strdup ("false");
        }

        return query;
}

static char *
make_oaf_query_with_known_mime_type (const char *mime_type, 
				     const char *uri_scheme, 
				     GList      *explicit_iids, 
				     const char *extra_requirements)
{
        char *mime_supertype;
        char *result;
        char *explicit_iid_query;

        mime_supertype = mime_type_get_supertype (mime_type);

        explicit_iid_query = make_oaf_query_for_explicit_content_view_iids (explicit_iids);

        result = g_strdup_printf 
                (
                 /* Check if the component has the interfaces we need.
                  * We can work with either a Nautilus View, or
                  * with a Bonobo Control or Embeddable that supports
                  * one of the three persistence interfaces:
                  * PersistStream, ProgressiveDataSink, or
                  * PersistFile.
                  */
                 "(((repo_ids.has_all (['IDL:Bonobo/Control:1.0',"
                                      "'IDL:Nautilus/View:1.0'])"
                  "OR (repo_ids.has_one (['IDL:Bonobo/Control:1.0',"
                                         "'IDL:Bonobo/Embeddable:1.0'])"
                      "AND repo_ids.has_one (['IDL:Bonobo/PersistStream:1.0',"
                                             "'IDL:Bonobo/ProgressiveDataSink:1.0',"
                                             "'IDL:Bonobo/PersistFile:1.0'])))"
                 
                 /* Check that the component either has a specific
                  * MIME type or URI scheme. If neither is specified,
                  * then we don't trust that to mean "all MIME types
                  * and all schemes". For that, you have to do a
                  * wildcard for the MIME type or for the scheme.
                  */
                 "AND (bonobo:supported_mime_types.defined ()"
                      "OR bonobo:supported_uri_schemes.defined ()"
		      "OR bonobo:additional_uri_schemes.defined ())"

		 /* One of two possibilties */

		 /* FIXME bugzilla.gnome.org 42542: this comment is not very clear. */
		 /* 1 The mime type and URI scheme match the supported
                    attributes. */

		 "AND ("

                 /* Check that the supported MIME types include the
                  * URI's MIME type or its supertype.
                  */
                 "((NOT bonobo:supported_mime_types.defined ()"
                      "OR bonobo:supported_mime_types.has ('%s')"
                      "OR bonobo:supported_mime_types.has ('%s')"
                      "OR bonobo:supported_mime_types.has ('*/*'))"

                 /* Check that the supported URI schemes include the
                  * URI's scheme.
                  */
                 "AND (NOT bonobo:supported_uri_schemes.defined ()"
                      "OR bonobo:supported_uri_schemes.has ('%s')"
                      "OR bonobo:supported_uri_schemes.has ('*')))"

		 /* 2 OR The additional URI schemes include this URI's
		    scheme; if that is the case, this view applies
		    whether or not the mime type is supported. */

		 "OR (bonobo:additional_uri_schemes.has ('%s')"
                      "OR bonobo:additional_uri_schemes.has ('*')))"

                  /* Check that the component makes it clear that it's
                   * intended for Nautilus by providing a "view_as"
                   * name. We could instead support a default, but
                   * that would make components that are untested with
                   * Nautilus appear.  */
                 "AND nautilus:view_as_name.defined ())"

                  /* Also select iids that were specifically requested
                     for this location, even if they do not otherwise
                     meet the requirements. */
                  "OR %s)"

		 /* Make it possible to add extra requirements */
		 " AND (%s)"

                 /* The MIME type, MIME supertype, and URI scheme for
                  * the %s above.
                  */
                 , mime_type, mime_supertype, uri_scheme, uri_scheme

                 /* The explicit metafile iid query for the %s above. */
                 , explicit_iid_query

		 /* extra requirements */
		 , extra_requirements != NULL ? extra_requirements : "true");

        g_free (mime_supertype);
        g_free (explicit_iid_query);
        return result;
}

static char *
make_oaf_query_with_uri_scheme_only (const char *uri_scheme, 
				     GList      *explicit_iids, 
				     const char *extra_requirements)
{
        char *result;
        char *explicit_iid_query;
        
        explicit_iid_query = make_oaf_query_for_explicit_content_view_iids (explicit_iids);

        result = g_strdup_printf 
                (
                 /* Check if the component has the interfaces we need.
                  * We can work with either a Nautilus tView, or
                  * with a Bonobo Control or Embeddable that works on
                  * a file, which is indicated by Bonobo PersistFile.
                  */
                  "(((repo_ids.has_all(['IDL:Bonobo/Control:1.0',"
                                      "'IDL:Nautilus/View:1.0'])"
                   "OR (repo_ids.has_one(['IDL:Bonobo/Control:1.0',"
                                         "'IDL:Bonobo/Embeddable:1.0'])"
                       "AND repo_ids.has('IDL:Bonobo/PersistFile:1.0')))"


                  /* Check if the component supports this particular
                   * URI scheme.
                   */
                  "AND (((bonobo:supported_uri_schemes.has ('%s')"
                         "OR bonobo:supported_uri_schemes.has ('*'))"

                  /* Check that the component doesn't require
                   * particular MIME types. Note that even saying you support "all"
                   */
                  "AND (NOT bonobo:supported_mime_types.defined ()))"

		  /* FIXME bugzilla.gnome.org 42542: improve the comment explaining this. */
		  
		  /* This attribute allows uri schemes to be supported
		     even for unsupported mime types or no mime type. */
		  "OR (bonobo:additional_uri_schemes.has ('%s')"
		      "OR bonobo:additional_uri_schemes.has ('*')))"


                  /* Check that the component makes it clear that it's
                   * intended for Nautilus by providing a "view_as"
                   * name. We could instead support a default, but
                   * that would make components that are untested with
                   * Nautilus appear.  */
                  "AND nautilus:view_as_name.defined ())"

                 /* Also select iids that were specifically requested
                     for this location, even if they do not otherwise
                     meet the requirements. */

                  "OR %s)"

		 /* Make it possible to add extra requirements */
		  " AND (%s)"

                  /* The URI scheme for the %s above. */
                  , uri_scheme, uri_scheme

                  /* The explicit metafile iid query for the %s above. */
                  , explicit_iid_query,
		  extra_requirements != NULL ? extra_requirements : "true");

	g_free (explicit_iid_query);
	
        return result;
}



static GHashTable *
mime_type_list_to_hash_table (GList *types)
{
        GHashTable *result;
        GList *p;
        char *mime_type;

        result = g_hash_table_new (g_str_hash, g_str_equal);

        for (p = types; p != NULL; p = p->next) {
                if (p->data != NULL) {
                        mime_type = (char *) (p->data);
                        
			if (g_hash_table_lookup (result, mime_type) == NULL) {
#ifdef DEBUG_MJS
				printf ("XXX content mime type: %s\n", mime_type);
#endif
				g_hash_table_insert (result, g_strdup (mime_type), mime_type);
			}
                }
        }

        return result;
}

static void
free_key (gpointer key, 
	  gpointer value, 
	  gpointer user_data)
{
        g_free (key);
}

static void
mime_type_hash_table_destroy (GHashTable *table)
{
        g_hash_table_foreach (table, free_key, NULL);
        g_hash_table_destroy (table);
}



static gboolean
server_has_content_requirements (OAF_ServerInfo *server)
{
        OAF_Property *prop;
	
        prop = oaf_server_info_prop_find (server, "nautilus:required_directory_content_mime_types");

        if (prop == NULL || prop->v._d != OAF_P_STRINGV) {
                return FALSE;
        } else {
		return TRUE;
	}
}

static gboolean
server_matches_content_requirements (OAF_ServerInfo *server, 
				     GHashTable     *type_table, 
				     GList          *explicit_iids)
{
        OAF_Property *prop;
        GNOME_stringlist types;
        guint i;

        /* Components explicitly requested in the metafile are not capability tested. */
        if (g_list_find_custom (explicit_iids, (gpointer) server->iid, (GCompareFunc) strcmp) != NULL) {
                return TRUE;
        }

        if (!server_has_content_requirements (server)) {
                return TRUE;
        } else {
        	prop = oaf_server_info_prop_find (server, "nautilus:required_directory_content_mime_types");

                types = prop->v._u.value_stringv;

                for (i = 0; i < types._length; i++) {
                        if (g_hash_table_lookup (type_table, types._buffer[i]) != NULL) {
                                return TRUE;
                        }
                }
        }

        return FALSE;
}


static char *nautilus_sort_criteria[] = {
        /* Prefer anything else over the loser view. */
        "iid != 'OAFIID:nautilus_content_loser:95901458-c68b-43aa-aaca-870ced11062d'",
        /* Prefer anything else over the sample view. */
        "iid != 'OAFIID:nautilus_sample_content_view:45c746bc-7d64-4346-90d5-6410463b43ae'",
	/* Sort alphabetically */
	"name",
        NULL};



static GList *
nautilus_do_component_query (const char        *mime_type, 
			     const char        *uri_scheme, 
			     GList             *item_mime_types,
			     gboolean           ignore_content_mime_types,
			     GList             *explicit_iids,
			     char             **extra_sort_criteria,
			     char              *extra_requirements)
{ 
	OAF_ServerInfoList *oaf_result;
	char *query;
	GList *retval;
	char **all_sort_criteria;
	CORBA_Environment ev;

        oaf_result = NULL;
        query = NULL;

        if (is_known_mime_type (mime_type)) {
                query = make_oaf_query_with_known_mime_type (mime_type, uri_scheme, explicit_iids, extra_requirements);
        } else {
                query = make_oaf_query_with_uri_scheme_only (uri_scheme, explicit_iids, extra_requirements);
        }

	all_sort_criteria = strv_concat (extra_sort_criteria, nautilus_sort_criteria);

	CORBA_exception_init (&ev);

	oaf_result = oaf_query (query, all_sort_criteria, &ev);
	
	g_free (all_sort_criteria);
	g_free (query);

	retval = NULL;

        if (ev._major == CORBA_NO_EXCEPTION && oaf_result != NULL && oaf_result->_length > 0) {
                GHashTable *content_types;
                guint i;
           
                content_types = mime_type_list_to_hash_table (item_mime_types);
                
                for (i = 0; i < oaf_result->_length; i++) {
                        OAF_ServerInfo *server;

                        server = &oaf_result->_buffer[i];

                        if (ignore_content_mime_types || 
			    server_matches_content_requirements (server, content_types, explicit_iids)) {
                                /* Hack to suppress the Bonobo_Sample_Text component, since the Nautilus text
                                 * view is a superset and it's confusing for the user to be presented with both
                                 */
                                if (server->iid != NULL && strcmp (server->iid, "OAFIID:Bonobo_Sample_Text") != 0) {
                                	retval = g_list_prepend
                                        	(retval, 
						 OAF_ServerInfo_duplicate (server));
                        	}
                        }
                }

                mime_type_hash_table_destroy (content_types);
        }

	CORBA_free (oaf_result);

	CORBA_exception_free (&ev);
	
	return g_list_reverse (retval);
}


static GList *
str_list_difference (GList *a, 
		     GList *b)
{
	GList *p;
	GList *retval;

	retval = NULL;

	for (p = a; p != NULL; p = p->next) {
		if (g_list_find_custom (b, p->data, (GCompareFunc) strcmp) == NULL) {
			retval = g_list_prepend (retval, p->data);
		}
	}

	retval = g_list_reverse (retval);
	return retval;
}


static int
strv_length (char **a)
{
	int i;

	for (i = 0; a != NULL && a[i] != NULL; i++) {
	}

	return i;
}

static char **
strv_concat (char **a, 
	     char **b)
{
	int a_length;
	int b_length;
	int i;
	int j;
	
	char **result;

	a_length = strv_length (a);
	b_length = strv_length (b);

	result = g_new0 (char *, a_length + b_length + 1);
	
	j = 0;

	for (i = 0; a != NULL && a[i] != NULL; i++) {
		result[j] = a[i];
		j++;
	}

	for (i = 0; b != NULL && b[i] != NULL; i++) {
		result[j] = b[i];
		j++;
	}

	result[j] = NULL;

	return result;
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
	    && g_strcasecmp ((const char *) uri_scheme, "file") == 0) {
		return TRUE;
	}
	return g_list_find_custom (application->supported_uri_schemes,
				   uri_scheme,
				   eel_strcasecmp_compare_func) != NULL;
}
