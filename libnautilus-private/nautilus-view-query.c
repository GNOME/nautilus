/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-view-query.c - view queries for directories

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
#include "nautilus-view-query.h"
 
#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include "nautilus-global-preferences.h"
#include "nautilus-mime-actions.h"
#include <bonobo-activation/bonobo-activation-activate.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <stdio.h>

static char       *mime_type_get_supertype                       (const char               *mime_type);
static GList      *get_explicit_content_view_iids_from_metafile  (NautilusFile             *file);
static gboolean    server_has_content_requirements               (Bonobo_ServerInfo           *server);
static GList      *nautilus_do_component_query                   (const char               *mime_type,
								  const char               *uri_scheme,
								  GList                    *content_mime_types,
								  gboolean                  ignore_content_mime_types,
								  GList                    *explicit_iids,
								  char                    **extra_sort_criteria,
								  char                     *extra_requirements,
								  gboolean                  must_be_view);
static char      **strv_concat                                   (char                    **a,
								  char                    **b);

static gboolean
is_known_mime_type (const char *mime_type)
{
	return eel_strcasecmp (mime_type, GNOME_VFS_MIME_TYPE_UNKNOWN) != 0;
}

static gboolean
nautilus_view_query_check_if_minimum_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_minimum_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

static gboolean
nautilus_view_query_check_if_full_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_full_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

static NautilusFileAttributes 
nautilus_view_query_get_popup_file_attributes (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_VOLUMES |
		NAUTILUS_FILE_ATTRIBUTE_ACTIVATION_URI |
		NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE;
}

static gboolean
nautilus_view_query_check_if_popup_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_view_query_get_popup_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

static char **
nautilus_view_query_get_default_component_sort_conditions (NautilusFile *file, char *default_component_string)
{
	char **sort_conditions;
	char *supertype;
	char *mime_type;
	
	sort_conditions = g_new0 (char *, 4);
	
	mime_type = nautilus_file_get_mime_type (file);

	supertype = mime_type_get_supertype (mime_type);

	/* prefer the exact right IID */
	if (default_component_string != NULL) {
		sort_conditions[0] = g_strconcat ("iid == '", default_component_string, "'", NULL);
	} else {
		sort_conditions[0] = g_strdup ("true");
	}

	/* Prefer something that matches the exact type to something
	   that matches the supertype */
	if (is_known_mime_type (mime_type)) {
		sort_conditions[1] = g_strconcat ("bonobo:supported_mime_types.has ('",mime_type,"')", NULL);
	} else {
		sort_conditions[1] = g_strdup ("true");
	}

	/* Prefer something that matches the supertype to something that matches `*' */
	if (is_known_mime_type (mime_type) && supertype != NULL) {
		sort_conditions[2] = g_strconcat ("bonobo:supported_mime_types.has ('",supertype,"')", NULL);
	} else {
		sort_conditions[2] = g_strdup ("true");
	}

	sort_conditions[3] = NULL;

	g_free (mime_type);
	g_free (supertype);

	return sort_conditions;
}	

static Bonobo_ServerInfo *
nautilus_view_query_get_default_component_for_file_internal (NautilusFile *file,
							     gboolean      fallback)
{
	GList *info_list;
	char *default_component_string;
	char *mime_type;
	char *uri_scheme;
	GList *item_mime_types;
	GList *explicit_iids;
	Bonobo_ServerInfo *server;
	char **sort_conditions;
	char *extra_requirements;
	gboolean metadata_default;

	if (!nautilus_view_query_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	info_list = NULL;

	mime_type = nautilus_file_get_mime_type (file);

	uri_scheme = nautilus_file_get_uri_scheme (file);

	explicit_iids = get_explicit_content_view_iids_from_metafile (file); 

	if (!nautilus_view_query_check_if_full_attributes_ready (file) || 
	    !nautilus_file_get_directory_item_mime_types (file, &item_mime_types)) {
		item_mime_types = NULL;
	}

	default_component_string = NULL;
	if (!fallback) {
		default_component_string = nautilus_file_get_metadata 
			(file, NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT, NULL);
	} 

    	if (default_component_string == NULL) {
		metadata_default = FALSE;

		if (nautilus_file_is_directory (file)) {
			default_component_string = nautilus_global_preferences_get_default_folder_viewer_preference_as_iid ();
		} else {
			g_warning ("Trying to load view for non-directory");
			/* Default component chosen based only on type. */
		}
	} else {
		metadata_default = TRUE;
	}
	
	sort_conditions = nautilus_view_query_get_default_component_sort_conditions (file, default_component_string);

	/* If the default is specified in the per-uri metadata,
           respect the setting regardless of content type requirements */
	if (metadata_default) {
		extra_requirements = g_strconcat ("iid == '", default_component_string, "'", NULL);
		info_list = nautilus_do_component_query (mime_type, uri_scheme, item_mime_types, TRUE,
							 explicit_iids, sort_conditions, extra_requirements, TRUE);
		g_free (extra_requirements);
	}

	if (info_list == NULL) {
		info_list = nautilus_do_component_query (mime_type, uri_scheme, item_mime_types, FALSE, 
							 explicit_iids, sort_conditions, NULL, TRUE);
	}

	if (info_list != NULL) {
		server = Bonobo_ServerInfo_duplicate (info_list->data);
		gnome_vfs_mime_component_list_free (info_list);
	} else {
		server = NULL;		
	}
	
	eel_g_list_free_deep (item_mime_types);
	eel_g_list_free_deep (explicit_iids);
	g_strfreev (sort_conditions);

	g_free (uri_scheme);
	g_free (mime_type);
	g_free (default_component_string);

	return server;
}


Bonobo_ServerInfo *
nautilus_view_query_get_default_component_for_file (NautilusFile      *file)
{
	return nautilus_view_query_get_default_component_for_file_internal (file, FALSE);
}

Bonobo_ServerInfo *
nautilus_view_query_get_fallback_component_for_file (NautilusFile      *file)
{
	return nautilus_view_query_get_default_component_for_file_internal (file, TRUE);
}


GList *
nautilus_view_query_get_components_for_file (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *item_mime_types;
	GList *info_list;
	GList *explicit_iids;

	if (!nautilus_view_query_check_if_minimum_attributes_ready (file)) {
		return NULL;
	}

	uri_scheme = nautilus_file_get_uri_scheme (file);

	mime_type = nautilus_file_get_mime_type (file);
	explicit_iids = get_explicit_content_view_iids_from_metafile (file); 

	if (!nautilus_view_query_check_if_full_attributes_ready (file) || 
	    !nautilus_file_get_directory_item_mime_types (file, &item_mime_types)) {
		item_mime_types = NULL;
	}

	info_list = nautilus_do_component_query (mime_type, uri_scheme,
						 item_mime_types, FALSE,
						 explicit_iids, NULL,
						 NULL, TRUE);
	
	eel_g_list_free_deep (explicit_iids);
	eel_g_list_free_deep (item_mime_types);

	g_free (uri_scheme);
	g_free (mime_type);

	return info_list;
}

GnomeVFSResult
nautilus_view_query_set_default_component_for_file (NautilusFile      *file,
						    const char        *component_iid)
{
	g_return_val_if_fail (nautilus_view_query_check_if_minimum_attributes_ready (file), 
			      GNOME_VFS_ERROR_GENERIC);

	nautilus_file_set_metadata 
		(file, NAUTILUS_METADATA_KEY_DEFAULT_COMPONENT, NULL, component_iid);

	return GNOME_VFS_OK;
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
make_bonobo_activation_query_for_explicit_content_view_iids (GList *view_iids)
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
make_bonobo_activation_query_with_known_mime_type (const char *mime_type, 
						   const char *uri_scheme, 
						   GList      *explicit_iids, 
						   const char *extra_requirements,
						   gboolean    must_be_view)
{
        char *mime_supertype;
        char *result;
        char *explicit_iid_query;
	const char *view_as_name_logic;

        mime_supertype = mime_type_get_supertype (mime_type);

        explicit_iid_query = make_bonobo_activation_query_for_explicit_content_view_iids (explicit_iids);

	if (must_be_view) {
		view_as_name_logic = "nautilus:view_as_name.defined ()";
	} else {
		view_as_name_logic = "true";
	}

        result = g_strdup_printf 
                (

                 
                 
                 /* Check that the component either has a specific
                  * MIME type or URI scheme. If neither is specified,
                  * then we don't trust that to mean "all MIME types
                  * and all schemes". For that, you have to do a
                  * wildcard for the MIME type or for the scheme.
                  */
                 "(bonobo:supported_mime_types.defined ()"
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
		  "AND %s)"
                  

                  /* Also select iids that were specifically requested
                     for this location, even if they do not otherwise
                     meet the requirements. */
                  "OR %s)"

		 /* Make it possible to add extra requirements */
		 " AND (%s)"

                 /* The MIME type, MIME supertype, and URI scheme for
                  * the %s above.
                  */
                 , mime_type, mime_supertype, uri_scheme, uri_scheme,

                 /* The explicit metafile iid query for the %s above. */
                 view_as_name_logic, explicit_iid_query

		 /* extra requirements */
		 , extra_requirements != NULL ? extra_requirements : "true");

	if (must_be_view) {
		char *str;


                 /* Check if the component has the interfaces we need.
                  * We can work with either a Nautilus View, or
                  * with a Bonobo Control or Embeddable that supports
                  * one of the three persistence interfaces:
                  * PersistStream, ProgressiveDataSink, or
                  * PersistFile.
                  */
		str = g_strdup_printf ("(((repo_ids.has_all (['IDL:Bonobo/Control:1.0',"
                                      "'IDL:Nautilus/View:1.0'])"
                  "OR (repo_ids.has_one (['IDL:Bonobo/Control:1.0',"
                                         "'IDL:Bonobo/Embeddable:1.0'])"
                      "AND repo_ids.has_one (['IDL:Bonobo/PersistStream:1.0',"
                                             "'IDL:Bonobo/ProgressiveDataSink:1.0',"
                                          "'IDL:Bonobo/PersistFile:1.0']))) "
					  "AND %s", result);
		g_free (result);
		result = str;
	} else {
		char *str;
		str = g_strdup_printf ("((%s", result);
		g_free (result);
		result = str;
	}

        g_free (mime_supertype);
        g_free (explicit_iid_query);
        return result;
}

static char *
make_bonobo_activation_query_with_uri_scheme_only (const char *uri_scheme, 
				     GList      *explicit_iids, 
				     const char *extra_requirements,
				     gboolean    must_be_view)
{
        char *result;
        char *explicit_iid_query;
	const char *view_as_name_logic;
        
        explicit_iid_query = make_bonobo_activation_query_for_explicit_content_view_iids (explicit_iids);

	if (must_be_view) {
		view_as_name_logic = "nautilus:view_as_name.defined ()";
	} else {
		view_as_name_logic = "true";
	}

        result = g_strdup_printf 
                (

                  /* Check if the component supports this particular
                   * URI scheme.
                   */
                  "(((bonobo:supported_uri_schemes.has ('%s')"
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
		  "AND %s)"

                 /* Also select iids that were specifically requested
                     for this location, even if they do not otherwise
                     meet the requirements. */

                  "OR %s)"

		 /* Make it possible to add extra requirements */
		  " AND (%s)"

                  /* The URI scheme for the %s above. */
                  , uri_scheme, uri_scheme, view_as_name_logic

                  /* The explicit metafile iid query for the %s above. */
                  , explicit_iid_query,
		  extra_requirements != NULL ? extra_requirements : "true");
	

	if (must_be_view) {
		char *str;


                 /* Check if the component has the interfaces we need.
                  * We can work with either a Nautilus View, or
                  * with a Bonobo Control or Embeddable that supports
                  * one of the three persistence interfaces:
                  * PersistStream, ProgressiveDataSink, or
                  * PersistFile.
                  */
		str = g_strdup_printf ("(((repo_ids.has_all (['IDL:Bonobo/Control:1.0',"
                                      "'IDL:Nautilus/View:1.0'])"
                  "OR (repo_ids.has_one (['IDL:Bonobo/Control:1.0',"
                                         "'IDL:Bonobo/Embeddable:1.0'])"
                      "AND repo_ids.has_one (['IDL:Bonobo/PersistStream:1.0',"
                                             "'IDL:Bonobo/ProgressiveDataSink:1.0',"
                                          "'IDL:Bonobo/PersistFile:1.0']))) "
					  "AND %s", result);
		g_free (result);
		result = str;
	} else {
		char *str;
		str = g_strdup_printf ("((%s", result);
		g_free (result);
		result = str;
	}
		  

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
server_has_content_requirements (Bonobo_ServerInfo *server)
{
        Bonobo_ActivationProperty *prop;
	
        prop = bonobo_server_info_prop_find (server, "nautilus:required_directory_content_mime_types");

        if (prop == NULL || prop->v._d != Bonobo_ACTIVATION_P_STRINGV) {
                return FALSE;
        } else {
		return TRUE;
	}
}

static gboolean
server_matches_content_requirements (Bonobo_ServerInfo *server, 
				     GHashTable     *type_table, 
				     GList          *explicit_iids)
{
        Bonobo_ActivationProperty *prop;
        Bonobo_StringList types;
        guint i;

        /* Components explicitly requested in the metafile are not capability tested. */
        if (g_list_find_custom (explicit_iids, (gpointer) server->iid, (GCompareFunc) strcmp) != NULL) {
                return TRUE;
        }

        if (!server_has_content_requirements (server)) {
                return TRUE;
        } else {
        	prop = bonobo_server_info_prop_find (server, "nautilus:required_directory_content_mime_types");

                types = prop->v._u.value_stringv;

                for (i = 0; i < types._length; i++) {
                        if (g_hash_table_lookup (type_table, types._buffer[i]) != NULL) {
                                return TRUE;
                        }
                }
        }

        return FALSE;
}


/* FIXME: do we actually need this it would seem to me that the
 * test_only attribute handles this
 */
static char *nautilus_sort_criteria[] = {
        /* Prefer anything else over the loser view. */
        "iid != 'OAFIID:Nautilus_Content_Loser'",
        /* Prefer anything else over the sample view. */
        "iid != 'OAFIID:Nautilus_Sample_Content_View'",
	/* Sort alphabetically */
	"name",
        NULL
};

static GList *
nautilus_do_component_query (const char        *mime_type, 
			     const char        *uri_scheme, 
			     GList             *item_mime_types,
			     gboolean           ignore_content_mime_types,
			     GList             *explicit_iids,
			     char             **extra_sort_criteria,
			     char              *extra_requirements,
			     gboolean           must_be_view)
{ 
	Bonobo_ServerInfoList *bonobo_activation_result;
	char *query;
	GList *retval;
	char **all_sort_criteria;
	CORBA_Environment ev;

        bonobo_activation_result = NULL;
        query = NULL;

        if (is_known_mime_type (mime_type)) {
                query = make_bonobo_activation_query_with_known_mime_type (mime_type, uri_scheme, explicit_iids, extra_requirements, must_be_view);
        } else {
                query = make_bonobo_activation_query_with_uri_scheme_only (uri_scheme, explicit_iids, extra_requirements, must_be_view);
        }

	all_sort_criteria = strv_concat (extra_sort_criteria, nautilus_sort_criteria);

	CORBA_exception_init (&ev);

	bonobo_activation_result = bonobo_activation_query (query, all_sort_criteria, &ev);
	
	g_free (all_sort_criteria);
	g_free (query);

	retval = NULL;

        if (ev._major == CORBA_NO_EXCEPTION && bonobo_activation_result != NULL && bonobo_activation_result->_length > 0) {
                GHashTable *content_types;
                guint i;
           
                content_types = mime_type_list_to_hash_table (item_mime_types);
                
                for (i = 0; i < bonobo_activation_result->_length; i++) {
                        Bonobo_ServerInfo *server;

                        server = &bonobo_activation_result->_buffer[i];

                        if (ignore_content_mime_types || 
			    server_matches_content_requirements (server, content_types, explicit_iids)) {
                                if (server->iid != NULL) {
                                	retval = g_list_prepend
                                        	(retval, 
						 Bonobo_ServerInfo_duplicate (server));
                        	}
                        }
                }

                mime_type_hash_table_destroy (content_types);
        }

	CORBA_free (bonobo_activation_result);

	CORBA_exception_free (&ev);
	
	return g_list_reverse (retval);
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

GList *
nautilus_view_query_get_popup_components_for_file (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	char *extra_reqs;
	GList *item_mime_types;
	GList *info_list;

	if (!nautilus_view_query_check_if_popup_attributes_ready (file)) {
		return NULL;
	}

	uri_scheme = nautilus_file_get_uri_scheme (file);

	mime_type = nautilus_file_get_mime_type (file);

	if (!nautilus_view_query_check_if_full_attributes_ready (file) || 
	    !nautilus_file_get_directory_item_mime_types (file, &item_mime_types)) {
		item_mime_types = NULL;
	}

	extra_reqs = "repo_ids.has ('IDL:Bonobo/Listener:1.0') AND (nautilus:context_menu_handler == true) AND nautilus:can_handle_multiple_files.defined()";

	info_list = nautilus_do_component_query (mime_type, uri_scheme,
						 item_mime_types, FALSE,
						 NULL, NULL,
						 extra_reqs, FALSE);
	
	eel_g_list_free_deep (item_mime_types);

	g_free (uri_scheme);
	g_free (mime_type);

	return info_list;
}

GList *
nautilus_view_query_get_property_components_for_file (NautilusFile *file)
{
        char *mime_type;
        char *uri_scheme;
        char *extra_reqs;
        GList *item_mime_types;
        GList *info_list;

        if (!nautilus_view_query_check_if_minimum_attributes_ready (file)) {
                return NULL;
        }

        uri_scheme = nautilus_file_get_uri_scheme (file);

        mime_type = nautilus_file_get_mime_type (file);

        if (!nautilus_view_query_check_if_full_attributes_ready (file) ||
            !nautilus_file_get_directory_item_mime_types (file, &item_mime_types
)) {
                item_mime_types = NULL;
        }

        extra_reqs = "repo_ids.has ('IDL:Bonobo/Control:1.0') AND nautilus:property_page_name.defined()";

        info_list = nautilus_do_component_query (mime_type, uri_scheme,
                                                 item_mime_types, FALSE,
                                                 NULL, NULL,
                                                 extra_reqs, FALSE);

        eel_g_list_free_deep (item_mime_types);

        g_free (uri_scheme);
        g_free (mime_type);

        return info_list;
}     

static gboolean
has_server_info_in_list (GList *list, Bonobo_ServerInfo *info)
{
	for (; list; list = list->next) {
		Bonobo_ServerInfo *tmp_info = list->data;

		if (strcmp (tmp_info->iid, info->iid) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static GList *
server_info_list_intersection (GList *a, GList *b)
{
	GList *result = NULL;

	if (a == NULL || b == NULL) {
		return NULL;
	}

	while (b) {
		Bonobo_ServerInfo *info;
		
		info = (Bonobo_ServerInfo *)b->data;

		if (has_server_info_in_list (a, info)) {
			result = g_list_prepend (result,
				   Bonobo_ServerInfo_duplicate (info));

		}
		
		b = b->next;
	}

	return g_list_reverse (result);
}

GList *
nautilus_view_query_get_property_components_for_files (GList *files)
{
	GList *result, *l;

	result = NULL;

	for (l = files; l; l = l->next) {
		GList *components, *new_result;

		components = nautilus_view_query_get_property_components_for_file (l->data);
		if (result != NULL) {
			new_result = server_info_list_intersection (result,
								    components);
			gnome_vfs_mime_component_list_free (result);
			gnome_vfs_mime_component_list_free (components);
			result = new_result;
		} else {
			result = components;;
		}


	}	

	return result;
}

GList *
nautilus_view_query_get_popup_components_for_files (GList *files)
{
	GList *result, *l;

	result = NULL;

	for (l = files; l; l = l->next) {
		GList *components, *new_result;

		components = nautilus_view_query_get_popup_components_for_file (l->data);
		if (result != NULL) {
			new_result = server_info_list_intersection (result,
								   components);
			gnome_vfs_mime_component_list_free (result);
			gnome_vfs_mime_component_list_free (components);
			result = new_result;
		} else {
			result = components;;
		}


	}	

	return result;
}
