/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-actions.h - uri-specific versions of mime action functions

   Copyright (C) 2000 Eazel, Inc.

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

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-file-attributes.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-metadata.h"

#include <stdio.h>


static gint gnome_vfs_mime_application_has_id (GnomeVFSMimeApplication *application, const char *id);
static gint gnome_vfs_mime_id_matches_application (const char *id, GnomeVFSMimeApplication *application);
static gboolean gnome_vfs_mime_application_has_id_not_in_list (GnomeVFSMimeApplication *application, GList *ids);
static gboolean string_not_in_list (const char *str, GList *list);
static char *extract_prefix_add_suffix (const char *string, const char *separator, const char *suffix);
static char *mime_type_get_supertype (const char *mime_type);
static char *uri_string_get_scheme (const char *uri_string);
static GList *get_explicit_content_view_iids_from_metafile (NautilusDirectory *directory);
static char *make_oaf_query_for_explicit_content_view_iids (GList *view_iids);
static char *make_oaf_query_with_known_mime_type (const char *mime_type, const char *uri_scheme, GList *explicit_iids, const char *extra_requirements);
static char *make_oaf_query_with_uri_scheme_only (const char *uri_scheme, GList *explicit_iids, const char *extra_requirements);
static GHashTable *file_list_to_mime_type_hash_table (GList *files);
static void free_key (gpointer key, gpointer value, gpointer user_data);
static void mime_type_hash_table_destroy (GHashTable *table);
static gboolean server_matches_content_requirements (OAF_ServerInfo *server, 
						     GHashTable *type_table, 
						     GList *explicit_iids);
static GList *nautilus_do_component_query (const char *mime_type, 
					   const char *uri_scheme, 
					   GList *files,
					   GList *explicit_iids,
					   char **extra_sort_criteria,
					   char *extra_requirements,
					   CORBA_Environment *ev);
static GList *str_list_difference   (GList *a, 
				     GList *b);
static char *get_mime_type_from_uri (const char 
				     *text_uri);

static int strv_length (char **a);
static char **strv_concat (char **a, char **b);



GnomeVFSMimeActionType
nautilus_mime_get_default_action_type_for_uri (const char *uri)
{
	char *mime_type;
	NautilusDirectory *directory;
	char *action_type_string;
	GnomeVFSMimeActionType action_type;

	directory = nautilus_directory_get (uri);
	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	action_type_string = nautilus_directory_get_metadata
		(directory, NAUTILUS_METADATA_KEY_DEFAULT_ACTION_TYPE, NULL);
	nautilus_directory_unref (directory);

	if (action_type_string == NULL) {
		/* FIXME bugzilla.eazel.com 1263: 
		   need NautilusDirectory cover for getting
                   mime type, and need to tie it into the
                   call_when_ready interface. Would want to use it
                   here. That way we won't be computing the mime type
                   over and over in the process of finding info on a
                   target URI. */
		mime_type = get_mime_type_from_uri (uri);
		action_type = gnome_vfs_mime_get_default_action_type (mime_type);
		g_free (mime_type);
		return action_type;
	} else {
		if (strcasecmp (action_type_string, "application") == 0) {
			return GNOME_VFS_MIME_ACTION_TYPE_APPLICATION;
		} else if (strcasecmp (action_type_string, "component") == 0) {
			return GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;
		} else {
			return GNOME_VFS_MIME_ACTION_TYPE_NONE;
		}
	}
}

GnomeVFSMimeAction *
nautilus_mime_get_default_action_for_uri (const char *uri)
{
	GnomeVFSMimeAction *action;

	action = g_new0 (GnomeVFSMimeAction, 1);

	action->action_type = nautilus_mime_get_default_action_type_for_uri (uri);

	switch (action->action_type) {
	case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
		action->action.application = 
			nautilus_mime_get_default_application_for_uri (uri);
		if (action->action.application == NULL) {
			g_free (action);
			action = NULL;
		}
		break;
	case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
		action->action.component = 
			nautilus_mime_get_default_component_for_uri (uri);
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
nautilus_mime_get_default_application_for_uri_internal (const char *uri, gboolean *user_chosen)
{
	char *mime_type;
	GnomeVFSMimeApplication *result;
	NautilusDirectory *directory;
	char *default_application_string;
	gboolean used_user_chosen_info;

	used_user_chosen_info = TRUE;

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	default_application_string = nautilus_directory_get_metadata 
		(directory, NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION, NULL);
	nautilus_directory_unref (directory);

	if (default_application_string == NULL) {
		/* FIXME bugzilla.eazel.com 1263: 
		   need NautilusDirectory cover for getting
                   mime type, and need to tie it into the
                   call_when_ready interface. Would want to use it
                   here. That way we won't be computing the mime type
                   over and over in the process of finding info on a
                   target URI. */
		mime_type = get_mime_type_from_uri (uri);
		result = gnome_vfs_mime_get_default_application (mime_type);
		g_free (mime_type);
		used_user_chosen_info = FALSE;
	} else {
		result = gnome_vfs_mime_application_new_from_id (default_application_string);
	}

	if (user_chosen != NULL) {
		*user_chosen = used_user_chosen_info;
	}

	return result;
}

GnomeVFSMimeApplication *
nautilus_mime_get_default_application_for_uri (const char *uri)
{
	return nautilus_mime_get_default_application_for_uri_internal (uri, NULL);
}

gboolean
nautilus_mime_is_default_application_for_uri_user_chosen (const char *uri)
{
	GnomeVFSMimeApplication *application;
	gboolean user_chosen;

	application = nautilus_mime_get_default_application_for_uri_internal (uri, &user_chosen);

	/* Doesn't count as user chosen if the user-specified data is bogus and doesn't
	 * result in an actual application.
	 */
	if (application == NULL) {
		return FALSE;
	}

	gnome_vfs_mime_application_free (application);
	 
	return user_chosen;
}


static OAF_ServerInfo *
nautilus_mime_get_default_component_for_uri_internal (const char *uri, gboolean *user_chosen)
{
	GList *info_list;
	OAF_ServerInfo *mime_default; 
	NautilusDirectory *directory;
	char *default_component_string;
	char *mime_type;
	char *uri_scheme;
	GList *files;
	GList *attributes;
	GList *explicit_iids;
	CORBA_Environment ev;
	OAF_ServerInfo *server;
	char *sort_conditions[5];
	char *supertype;
	gboolean used_user_chosen_info;
	GList *short_list;
	GList *p;
	char *prev;

	used_user_chosen_info = TRUE;

	CORBA_exception_init (&ev);

	mime_type = get_mime_type_from_uri (uri);
	uri_scheme = uri_string_get_scheme (uri);

	directory = nautilus_directory_get (uri);

        /* Arrange for all the file attributes we will need. */
        attributes = NULL;
        attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);

	files = nautilus_directory_wait_until_ready (directory, attributes, TRUE);
	default_component_string = nautilus_directory_get_metadata 
		(directory, NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT, NULL);
	explicit_iids = get_explicit_content_view_iids_from_metafile (directory); 
	g_list_free (attributes);
	nautilus_directory_unref (directory);

    	if (default_component_string == NULL && mime_type != NULL) {
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
	
	/* FIXME bugzilla.eazel.com 1264: 
	 * should return NULL for the following cases:
	 * - Known URI scheme pointing to nonexistent location
	 * - Syntactically invalid URI
	 */

	supertype = mime_type_get_supertype (mime_type);

	/* prefer the exact right IID */
	if (default_component_string != NULL) {
		sort_conditions[0] = g_strconcat ("iid == '", default_component_string, "'", NULL);
	} else {
		sort_conditions[0] = g_strdup ("true");
	}

	/* Prefer something from the short list */

	short_list = nautilus_mime_get_short_list_components_for_uri (uri);

	if (short_list != NULL) {
		sort_conditions[1] = g_strdup ("has (['");

		for (p = short_list; p != NULL; p = p->next) {
			prev = sort_conditions[1];
			
			if (p->next != NULL) {
				sort_conditions[1] = g_strconcat (prev, ((OAF_ServerInfo *) (p->data))->iid, 
								  "','", NULL);
			} else {
				sort_conditions[1] = g_strconcat (prev, ((OAF_ServerInfo *) (p->data))->iid, 
								  "'], iid)", NULL);
			}
			g_free (prev);
		}
	} else {
		sort_conditions[1] = g_strdup ("true");
	}
	
	gnome_vfs_mime_component_list_free (short_list);

	/* Prefer something that matches the exact type to something
	   that matches the supertype */
	if (mime_type != NULL) {
		sort_conditions[2] = g_strconcat ("bonobo:supported_mime_types.has ('",mime_type,"')", NULL);
	} else {
		sort_conditions[2] = g_strdup ("true");
	}

	/* Prefer something that matches the supertype to something that matches `*' */
	if (supertype != NULL) {
		sort_conditions[3] = g_strconcat ("bonobo:supported_mime_types.has ('",supertype,"')", NULL);
	} else {
		sort_conditions[3] = g_strdup ("true");
	}

	sort_conditions[4] = NULL;
	
	info_list = nautilus_do_component_query (mime_type, uri_scheme, files, explicit_iids, 
						 sort_conditions, NULL, &ev);
	

	if (ev._major == CORBA_NO_EXCEPTION  && info_list != NULL) {
		server = OAF_ServerInfo_duplicate (info_list->data);
		gnome_vfs_mime_component_list_free (info_list);

		if (default_component_string != NULL && strcmp (server->iid, default_component_string) == 0) {
			used_user_chosen_info = TRUE;	/* Default component chosen based on user-stored . */
		}
	} else {
		g_assert (info_list == NULL);  /* or else we are leaking it */
		server = NULL;
		
		/* FIXME bugzilla.eazel.com 1158: replace this
                   assertion with proper reporting of the error, once
                   the API supports error handling. */

		g_assert_not_reached ();
	}

	g_free (sort_conditions[0]);
	g_free (sort_conditions[1]);
	g_free (sort_conditions[2]);
	g_free (sort_conditions[3]);
	g_free (supertype);
	g_free (uri_scheme);
	g_free (mime_type);
	g_free (default_component_string);

	CORBA_exception_free (&ev);

	if (user_chosen != NULL) {
		*user_chosen = used_user_chosen_info;
	}

	return server;
}


OAF_ServerInfo *
nautilus_mime_get_default_component_for_uri (const char *uri)
{
	return nautilus_mime_get_default_component_for_uri_internal (uri, NULL);
}

gboolean
nautilus_mime_is_default_component_for_uri_user_chosen (const char *uri)
{
	OAF_ServerInfo *component;
	gboolean user_chosen;

	component = nautilus_mime_get_default_component_for_uri_internal (uri, &user_chosen);

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
nautilus_mime_get_short_list_applications_for_uri (const char *uri)
{
	char *mime_type;
	GList *result;
	GList *removed;
	NautilusDirectory *directory;
	GList *metadata_application_add_ids;
	GList *metadata_application_remove_ids;
	GList *p;
	GnomeVFSMimeApplication *application;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	metadata_application_add_ids = nautilus_directory_get_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_ADD, NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);
	metadata_application_remove_ids = nautilus_directory_get_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_REMOVE, NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);
	nautilus_directory_unref (directory);

	mime_type = get_mime_type_from_uri (uri);
	result = gnome_vfs_mime_get_short_list_applications (mime_type);
	g_free (mime_type);

	result = nautilus_g_list_partition (result, (NautilusGPredicateFunc) gnome_vfs_mime_application_has_id_not_in_list, 
					    metadata_application_remove_ids, &removed);

	gnome_vfs_mime_application_list_free (removed);

	for (p = metadata_application_add_ids; p != NULL; p = p->next) {
		if (g_list_find_custom (result,
					p->data,
					(GCompareFunc) gnome_vfs_mime_application_has_id) == NULL &&
		    g_list_find_custom (metadata_application_remove_ids,
					p->data,
					(GCompareFunc) strcmp) == NULL) {
			application = gnome_vfs_mime_application_new_from_id (p->data);

			if (application != NULL) {
				result = g_list_prepend (result, application);
			}
		}
	}

	CORBA_exception_free (&ev);

	/* FIXME bugzilla.eazel.com 1266: should sort alphabetically by name or something */
	return result;
}

GList *
nautilus_mime_get_short_list_components_for_uri (const char *uri)
{
	char *mime_type;
	char *uri_scheme;
	GList *servers;
	GList *iids;
	GList *result;
	GList *removed;
	NautilusDirectory *directory;
	GList *metadata_component_add_ids;
	GList *metadata_component_remove_ids;
	GList *p;
	OAF_ServerInfo *component;
	GList *attributes;
	GList *files;
	GList *explicit_iids;
	CORBA_Environment ev;
	char *extra_requirements;
	char *prev;

	CORBA_exception_init (&ev);

	uri_scheme = uri_string_get_scheme (uri);

	directory = nautilus_directory_get (uri);

        /* Arrange for all the file attributes we will need. */
        attributes = NULL;
        attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);

	files = nautilus_directory_wait_until_ready (directory, attributes, TRUE);
	explicit_iids = get_explicit_content_view_iids_from_metafile (directory); 
	g_list_free (attributes);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	metadata_component_add_ids = nautilus_directory_get_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_ADD, NAUTILUS_METADATA_SUBKEY_COMPONENT_IID);
	metadata_component_remove_ids = nautilus_directory_get_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_REMOVE, NAUTILUS_METADATA_SUBKEY_COMPONENT_IID);
	nautilus_directory_unref (directory);

	mime_type = get_mime_type_from_uri (uri);
	servers = gnome_vfs_mime_get_short_list_components (mime_type);
	iids = NULL;

	for (p = servers; p != NULL; p = p->next) {
		component = (OAF_ServerInfo *) p->data;

		iids = g_list_prepend (iids, component->iid);
	}

	iids = nautilus_g_list_partition
		(iids, (NautilusGPredicateFunc) string_not_in_list, 
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
		
	result = NULL;

	if (iids != NULL) {
		extra_requirements = g_strdup ("has (['");
		
		for (p = iids; p != NULL; p = p->next) {
			prev = extra_requirements;

			if (p->next != NULL) {
				extra_requirements = g_strconcat (prev, p->data, "','", NULL);
			} else {
				extra_requirements = g_strconcat (prev, p->data, "'], iid)", NULL);
			}

			g_free (prev);
		}


		result = nautilus_do_component_query (mime_type, uri_scheme, files, explicit_iids, NULL, extra_requirements, &ev);
		g_free (extra_requirements);
	}

	gnome_vfs_mime_component_list_free (servers);
	g_list_free (iids);
	g_free (uri_scheme);
	g_free (mime_type);
	
	return result;
}

gchar *
nautilus_mime_get_short_list_methods_for_uri (const char *uri)
{
	gchar *mime_type = get_mime_type_from_uri (uri);
	const gchar *method = gnome_vfs_mime_get_value (mime_type, "vfs-method");

	g_free(mime_type);

	if (method == NULL) return NULL;

	return g_strdup(method);
}


GList *
nautilus_mime_get_all_applications_for_uri (const char *uri)
{
	char *mime_type;
	GList *result;
	NautilusDirectory *directory;
	GList *metadata_application_ids;
	GList *p;
	GnomeVFSMimeApplication *application;

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	metadata_application_ids = nautilus_directory_get_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION, NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);
	nautilus_directory_unref (directory);

	mime_type = get_mime_type_from_uri (uri);

	result = gnome_vfs_mime_get_all_applications (mime_type);
	/* FIXME bugzilla.eazel.com 1268: 
	 * temporary hack; the non_uri code should do this merge 
	 */
	if (result == NULL) {
		result = gnome_vfs_mime_get_short_list_applications (mime_type);
	}

	for (p = metadata_application_ids; p != NULL; p = p->next) {
		if (!g_list_find_custom (result,
					 p->data,
					 (GCompareFunc) gnome_vfs_mime_application_has_id)) {
			application = gnome_vfs_mime_application_new_from_id (p->data);

			if (application != NULL) {
				result = g_list_prepend (result, application);
			}
		}
	}

	/* FIXME bugzilla.eazel.com 1266: should sort alphabetically by name or something */

	g_free (mime_type);
	return result;
}

gboolean
nautilus_mime_has_any_applications_for_uri (const char *uri)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_all_applications_for_uri (uri);
	result = list != NULL;
	gnome_vfs_mime_application_list_free (list);

	return result;
}

GList *
nautilus_mime_get_all_components_for_uri (const char *uri)
{
	char *mime_type;
	char *uri_scheme;
	GList *files;
	GList *attributes;
	GList *info_list;
	NautilusDirectory *directory;
	GList *explicit_iids;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	mime_type = get_mime_type_from_uri (uri);
	uri_scheme = uri_string_get_scheme (uri);

	directory = nautilus_directory_get (uri);

        /* Arrange for all the file attributes we will need. */
        attributes = NULL;
        attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);

	files = nautilus_directory_wait_until_ready (directory, attributes, TRUE);
	explicit_iids = get_explicit_content_view_iids_from_metafile (directory); 
	g_list_free (attributes);

	info_list = nautilus_do_component_query (mime_type, uri_scheme, files, explicit_iids, NULL, NULL, &ev);
	
	g_free (uri_scheme);
	g_free (mime_type);
	CORBA_exception_free (&ev);

	return info_list;
}

gboolean
nautilus_mime_has_any_components_for_uri (const char *uri)
{
	GList *list;
	gboolean result;

	list = nautilus_mime_get_all_components_for_uri (uri);
	result = list != NULL;
	gnome_vfs_mime_component_list_free (list);

	return result;
}

GnomeVFSResult
nautilus_mime_set_default_action_type_for_uri (const char             *uri,
					       GnomeVFSMimeActionType  action_type)
{
	NautilusDirectory *directory;
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

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	nautilus_directory_set_metadata 
		(directory, NAUTILUS_METADATA_KEY_DEFAULT_ACTION_TYPE, NULL, action_string);
	nautilus_directory_unref (directory);

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_default_application_for_uri (const char *uri,
					       const char *application_id)
{
	NautilusDirectory *directory;

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	nautilus_directory_set_metadata 
		(directory, NAUTILUS_METADATA_KEY_DEFAULT_APPLICATION, NULL, application_id);
	nautilus_directory_unref (directory);

	/* If there's no default action type, set it to match this. */
	if (application_id != NULL && 
	    nautilus_mime_get_default_action_type_for_uri (uri) == GNOME_VFS_MIME_ACTION_TYPE_NONE) {
		return nautilus_mime_set_default_action_type_for_uri (uri, GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
	}

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_default_component_for_uri (const char *uri,
					     const char *component_iid)
{
	NautilusDirectory *directory;

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	nautilus_directory_set_metadata 
		(directory, NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT, NULL, component_iid);
	nautilus_directory_unref (directory);

	/* If there's no default action type, set it to match this. */
	if (component_iid != NULL && 
	    nautilus_mime_get_default_action_type_for_uri (uri) == GNOME_VFS_MIME_ACTION_TYPE_NONE) {
		return nautilus_mime_set_default_action_type_for_uri (uri, GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
	}

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_short_list_applications_for_uri (const char *uri,
						   GList *applications)
{
	NautilusDirectory *directory;
	GList *add_list;
	GList *remove_list;
	GList *normal_short_list;
	GList *normal_short_list_ids;
	GList *p;
	char *mime_type;

	/* get per-mime short list */

	mime_type = get_mime_type_from_uri (uri);
	normal_short_list = gnome_vfs_mime_get_short_list_applications (mime_type);
	g_free (mime_type);

	normal_short_list_ids = NULL;
	for (p = normal_short_list; p != NULL; p = p->next) {
		normal_short_list_ids = g_list_prepend (normal_short_list_ids, ((GnomeVFSMimeApplication *) p->data)->id);
	}

	/* compute delta */

	add_list = str_list_difference (applications, normal_short_list_ids);
	remove_list = str_list_difference (normal_short_list_ids, applications);

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	nautilus_directory_set_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_ADD, NAUTILUS_METADATA_SUBKEY_APPLICATION_ID, add_list);
	nautilus_directory_set_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_APPLICATION_REMOVE, NAUTILUS_METADATA_SUBKEY_APPLICATION_ID, remove_list);
	nautilus_directory_unref (directory);	

	/* FIXME bugzilla.eazel.com 1269: 
	 * need to free normal_short_list, normal_short_list_ids, add_list, remove_list 
	 */

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_set_short_list_components_for_uri (const char *uri,
						 GList      *components)
{
	NautilusDirectory *directory;
	GList *add_list;
	GList *remove_list;
	GList *normal_short_list;
	GList *normal_short_list_ids;
	GList *p;
	char *mime_type;

	/* get per-mime short list */

	mime_type = get_mime_type_from_uri (uri);
	normal_short_list = gnome_vfs_mime_get_short_list_components (mime_type);
	g_free (mime_type);
	
	normal_short_list_ids = NULL;
	for (p = normal_short_list; p != NULL; p = p->next) {
		normal_short_list_ids = g_list_prepend (normal_short_list_ids, ((OAF_ServerInfo *) p->data)->iid);
	}

	/* compute delta */

	add_list = str_list_difference (components, normal_short_list_ids);
	remove_list = str_list_difference (normal_short_list_ids, components);

	directory = nautilus_directory_get (uri);

	nautilus_directory_wait_until_ready (directory, NULL, TRUE);
	nautilus_directory_set_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_ADD, NAUTILUS_METADATA_SUBKEY_COMPONENT_IID, add_list);
	nautilus_directory_set_metadata_list 
		(directory, NAUTILUS_METADATA_KEY_SHORT_LIST_COMPONENT_REMOVE, NAUTILUS_METADATA_SUBKEY_COMPONENT_IID, remove_list);
	nautilus_directory_unref (directory);	

	/* FIXME bugzilla.eazel.com 1269: 
	 * need to free normal_short_list, normal_short_list_ids, add_list, remove_list 
	 */

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_add_application_to_short_list_for_uri (const char *uri,
						     const char *application_id)
{
	GList *old_list, *new_list;
	GnomeVFSResult result;

	old_list = nautilus_mime_get_short_list_applications_for_uri (uri);

	if (!gnome_vfs_mime_id_in_application_list (application_id, old_list)) {
		new_list = g_list_append (gnome_vfs_mime_id_list_from_application_list (old_list), 
					  g_strdup (application_id));
		result = nautilus_mime_set_short_list_applications_for_uri (uri, new_list);
		nautilus_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_application_list_free (old_list);

	return result;
}

GnomeVFSResult
nautilus_mime_remove_application_from_short_list_for_uri (const char *uri,
							  const char *application_id)
{
	GList *old_list, *new_list;
	gboolean was_in_list;
	GnomeVFSResult result;

	old_list = nautilus_mime_get_short_list_applications_for_uri (uri);
	old_list = gnome_vfs_mime_remove_application_from_list
		(old_list, application_id, &was_in_list);

	if (!was_in_list) {
		result = GNOME_VFS_OK;
	} else {
		new_list = gnome_vfs_mime_id_list_from_application_list (old_list);
		result = nautilus_mime_set_short_list_applications_for_uri (uri, new_list);
		nautilus_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_application_list_free (old_list);
	
	return result;
}

GnomeVFSResult
nautilus_mime_add_component_to_short_list_for_uri (const char *uri,
						   const char *iid)
{
	GList *old_list, *new_list;
	GnomeVFSResult result;

	old_list = nautilus_mime_get_short_list_components_for_uri (uri);

	if (gnome_vfs_mime_id_in_component_list (iid, old_list)) {
		result = GNOME_VFS_OK;
	} else {
		new_list = g_list_append (gnome_vfs_mime_id_list_from_component_list (old_list), 
					  g_strdup (iid));
		result = nautilus_mime_set_short_list_components_for_uri (uri, new_list);
		nautilus_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_component_list_free (old_list);

	return result;
}

GnomeVFSResult
nautilus_mime_remove_component_from_short_list_for_uri (const char *uri,
							const char *iid)
{
	GList *old_list, *new_list;
	gboolean was_in_list;
	GnomeVFSResult result;

	old_list = nautilus_mime_get_short_list_components_for_uri (uri);
	old_list = gnome_vfs_mime_remove_component_from_list 
		(old_list, iid, &was_in_list);

	if (!was_in_list) {
		result = GNOME_VFS_OK;
	} else {
		new_list = gnome_vfs_mime_id_list_from_component_list (old_list);
		result = nautilus_mime_set_short_list_components_for_uri (uri, new_list);
		nautilus_g_list_free_deep (new_list);
	}

	gnome_vfs_mime_component_list_free (old_list);

	return result;
}

GnomeVFSResult
nautilus_mime_extend_all_applications_for_uri (const char *uri,
					       GList *applications)
{
	NautilusDirectory *directory;
	GList *metadata_application_ids;
	GList *extras;
	GList *final_applications;

	directory = nautilus_directory_get (uri);
	nautilus_directory_wait_until_ready (directory, NULL, TRUE);

	metadata_application_ids = nautilus_directory_get_metadata_list 
		(directory,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);

	extras = str_list_difference (applications, metadata_application_ids);

	final_applications = g_list_concat (g_list_copy (metadata_application_ids), extras);

	nautilus_directory_set_metadata_list 
		(directory,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID,
		 final_applications);

	nautilus_directory_unref (directory);

	return GNOME_VFS_OK;
}

GnomeVFSResult
nautilus_mime_remove_from_all_applications_for_uri (const char *uri,
						    GList *applications)
{
	NautilusDirectory *directory;
	GList *metadata_application_ids;
	GList *final_applications;

	directory = nautilus_directory_get (uri);
	nautilus_directory_wait_until_ready (directory, NULL, TRUE);

	metadata_application_ids = nautilus_directory_get_metadata_list 
		(directory,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID);

	final_applications = str_list_difference (metadata_application_ids, applications);

	nautilus_directory_set_metadata_list 
		(directory,
		 NAUTILUS_METADATA_KEY_EXPLICIT_APPLICATION,
		 NAUTILUS_METADATA_SUBKEY_APPLICATION_ID,
		 final_applications);
	nautilus_directory_unref (directory);

	return GNOME_VFS_OK;
}

static gint
gnome_vfs_mime_application_has_id (GnomeVFSMimeApplication *application, const char *id)
{
	return strcmp (application->id, id);
}

static gint
gnome_vfs_mime_id_matches_application (const char *id, GnomeVFSMimeApplication *application)
{
	return gnome_vfs_mime_application_has_id (application, id);
}

static gboolean
gnome_vfs_mime_application_has_id_not_in_list (GnomeVFSMimeApplication *application, GList *ids)
{
	return g_list_find_custom (ids, application, (GCompareFunc) gnome_vfs_mime_id_matches_application) == NULL;
}

static gboolean
string_not_in_list (const char *str, GList *list)
{
	return g_list_find_custom (list, (gpointer) str, (GCompareFunc) strcmp) == NULL;
}

static char *
extract_prefix_add_suffix (const char *string, const char *separator, const char *suffix)
{
        const char *separator_position;
        int prefix_length;
        char *result;

        separator_position = strstr (string, separator);
        prefix_length = separator_position == NULL
                ? strlen (string)
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

static char *
uri_string_get_scheme (const char *uri_string)
{
        return extract_prefix_add_suffix (uri_string, ":", "");
}



/*
 * The following routine uses metadata associated with the current url
 * to add content view components specified in the metadata. The
 * content views are specified in the string as <EXPLICIT_CONTENT_VIEW
 * IID="iid"/> elements inside the appropriate <DIRECTORY> or <FILE> element.  
 */

static GList *
get_explicit_content_view_iids_from_metafile (NautilusDirectory *directory)
{
        if (directory != NULL) {
                return nautilus_directory_get_metadata_list 
                        (directory, NAUTILUS_METADATA_KEY_EXPLICIT_COMPONENT, NAUTILUS_METADATA_SUBKEY_COMPONENT_IID);
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
make_oaf_query_with_known_mime_type (const char *mime_type, const char *uri_scheme, GList *explicit_iids, const char *extra_requirements)
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

		 /* FIXME: this comment is not very clear. */
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
make_oaf_query_with_uri_scheme_only (const char *uri_scheme, GList *explicit_iids, const char *extra_requirements)
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

		  /* FIXME: improve the comment explaining this. */
		  
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
           
        return result;
}



static GHashTable *
file_list_to_mime_type_hash_table (GList *files)
{
        GHashTable *result;
        GList *p;
        char *mime_type;

        result = g_hash_table_new (g_str_hash, g_str_equal);

        for (p = files; p != NULL; p = p->next) {
                if (p->data != NULL) {
                        mime_type = nautilus_file_get_mime_type ((NautilusFile *) p->data);
                        
                        if (NULL != mime_type) {
                                if (g_hash_table_lookup (result, mime_type) == NULL) {
#ifdef DEBUG_MJS
                                        printf ("XXX content mime type: %s\n", mime_type);
#endif
                                        g_hash_table_insert (result, mime_type, mime_type);
                                } else {
                                        g_free (mime_type);
                                }
                        }
                }
        }

        return result;
}

static void
free_key (gpointer key, gpointer value, gpointer user_data)
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
server_matches_content_requirements (OAF_ServerInfo *server, GHashTable *type_table, GList *explicit_iids)
{
        OAF_Property *prop;
        GNOME_stringlist types;
        int i;

        /* Components explicitly requested in the metafile are not capability tested. */
        if (g_list_find_custom (explicit_iids, (gpointer) server->iid, (GCompareFunc) strcmp) != NULL) {
                return TRUE;
        }

        prop = oaf_server_info_prop_find (server, "nautilus:required_directory_content_mime_types");

        if (prop == NULL || prop->v._d != OAF_P_STRINGV) {
                return TRUE;
        } else {
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

#if 0
        /* Prefer the industrial strengthe html viewer most */
        "iid == 'OAFIID:nautilus_mozilla_content_view:1ee70717-57bf-4079-aae5-922abdd576b1'",
        /* Prefer the gtkhtml viewer next */
        "iid == 'OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b'",
        /* Prefer the icon view next */
        "iid == 'OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058'",
#endif;




static GList *
nautilus_do_component_query (const char *mime_type, 
			     const char *uri_scheme, 
			     GList *files,
			     GList *explicit_iids,
			     char **extra_sort_criteria,
			     char *extra_requirements,
			     CORBA_Environment *ev)
{ 
	OAF_ServerInfoList *oaf_result;
	char *query;
	GList *retval;
	char **all_sort_criteria;

        oaf_result = NULL;
        query = NULL;

        
	if (mime_type != NULL) {
                query = make_oaf_query_with_known_mime_type (mime_type, uri_scheme, explicit_iids, extra_requirements);
        } else {
                query = make_oaf_query_with_uri_scheme_only (uri_scheme, explicit_iids, extra_requirements);
        }

#ifdef DEBUG_MJS
        printf ("query: \"%s\"\n", query);
#endif

	all_sort_criteria = strv_concat (extra_sort_criteria, nautilus_sort_criteria);;

	oaf_result = oaf_query (query, all_sort_criteria, ev);
		
	g_free (all_sort_criteria);
	g_free (query);

	retval = NULL;

        if (ev->_major == CORBA_NO_EXCEPTION && oaf_result != NULL && oaf_result->_length > 0) {
                GHashTable *content_types;
                int i;
           
                content_types = file_list_to_mime_type_hash_table (files);
                
                for (i = 0; i < oaf_result->_length; i++) {
                        OAF_ServerInfo *server;

                        server = &oaf_result->_buffer[i];

                        if (server_matches_content_requirements (server, content_types, explicit_iids)) {
                                retval = g_list_append
                                        (retval, 
                                         OAF_ServerInfo_duplicate (server));
                        }
                }

                mime_type_hash_table_destroy (content_types);
        } 

	CORBA_free (oaf_result);
	
	return retval;
}


static GList *
str_list_difference (GList *a, GList *b)
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


static char *
get_mime_type_from_uri (const char *text_uri)
{
        GnomeVFSURI *vfs_uri;
	GnomeVFSFileInfo *file_info;
	GnomeVFSResult result;
        const char *ctype;
	char *type;

	if (text_uri == NULL) {
		return NULL;
	}

	type = NULL;

	/* FIXME bugzilla.eazel.com 1263: 
	   A better way would be to get this info using
	   NautilusFile or NautilusDirectory or something, having
	   previously ensured that the info has been computed
	   async. */

        vfs_uri = gnome_vfs_uri_new (text_uri);

	if (vfs_uri != NULL) {
		file_info = gnome_vfs_file_info_new ();
		
		result = gnome_vfs_get_file_info_uri (vfs_uri, file_info,
						      GNOME_VFS_FILE_INFO_GET_MIME_TYPE
						      | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
		if (result == GNOME_VFS_OK) {
			ctype = gnome_vfs_file_info_get_mime_type (file_info);
			
			if (ctype != NULL) {
				type = g_strdup (ctype);
			}
			
			gnome_vfs_file_info_unref (file_info);
			gnome_vfs_uri_unref (vfs_uri);
		} 
	}

	return type;
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
strv_concat (char **a, char **b)
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
