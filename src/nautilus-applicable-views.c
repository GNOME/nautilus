/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* ntl-uri-map.c: Implementation of routines for mapping a location
   change request to a set of views and actual URL to be loaded. */

#include <config.h>
#include "ntl-uri-map.h"

#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-file.h>

#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

#include <liboaf/liboaf.h>

#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

#include "ntl-types.h"

/* forward declarations */
static void add_components_from_metadata        (NautilusNavigationInfo            *navinfo);
static void add_meta_view_iids_from_preferences (NautilusNavigationInfo            *navinfo);
static void async_get_file_info_text            (GnomeVFSAsyncHandle              **handle,
                                                 const char                        *text_uri,
                                                 GnomeVFSFileInfoOptions            options,
                                                 GnomeVFSAsyncGetFileInfoCallback   callback,
                                                 gpointer                           callback_data);

/* Nautilus View Identifiers associate a component name with a user displayable name */

static NautilusViewIdentifier *
nautilus_view_identifier_new (const char *iid, const char *name)
{
        NautilusViewIdentifier *new_identifier;
        
        g_return_val_if_fail (iid != NULL, NULL);
        g_return_val_if_fail (name != NULL, NULL);
        
        new_identifier = g_new0 (NautilusViewIdentifier, 1);
        new_identifier->iid = g_strdup (iid);
        new_identifier->name = g_strdup (name);
        
        return new_identifier;
}

static void
nautilus_view_identifier_free (NautilusViewIdentifier *identifier)
{
        if (identifier != NULL) {
                g_free (identifier->iid);
                g_free (identifier->name);
                g_free (identifier);
        }
}

static NautilusNavigationResult
get_nautilus_navigation_result_from_gnome_vfs_result (GnomeVFSResult gnome_vfs_result)
{
        switch (gnome_vfs_result) {
        case GNOME_VFS_OK:
                return NAUTILUS_NAVIGATION_RESULT_OK;
        case GNOME_VFS_ERROR_NOTFOUND:
        case GNOME_VFS_ERROR_HOSTNOTFOUND:
                return NAUTILUS_NAVIGATION_RESULT_NOT_FOUND;
        case GNOME_VFS_ERROR_INVALIDURI:
                return NAUTILUS_NAVIGATION_RESULT_INVALID_URI;
        case GNOME_VFS_ERROR_NOTSUPPORTED:
                return NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME;
        case GNOME_VFS_ERROR_GENERIC:
                /* This one has occurred at least once in the web browser component */
                return NAUTILUS_NAVIGATION_RESULT_UNSPECIFIC_ERROR;
        default:
                /* Whenever this message fires, we should consider adding a specific case
                 * to make the error as comprehensible as possible to the user. Please
                 * bug me (sullivan@eazel.com) if you see this fire and don't have the
                 * inclination to immediately make a good message yourself (tell me
                 * what GnomeVFSResult code the message reported, and what caused it to
                 * fire).
                 */
                g_message ("in ntl-uri-map.c, got unhandled GnomeVFSResult %d", gnome_vfs_result);
                return NAUTILUS_NAVIGATION_RESULT_UNSPECIFIC_ERROR;
        }
}

/* GCompareFunc-style function for checking whether a given string matches
 * the iid of a NautilusViewIdentifier. Returns 0 if there is a match.
 */
static int
check_iid (gconstpointer a, gconstpointer b)
{
	NautilusViewIdentifier *identifier;
	char *string;
        
	identifier = (NautilusViewIdentifier *)a;
	string = (char *)b;
        
	return strcmp (identifier->iid, string) != 0;
}

/**
 * set_initial_content_iid:
 * 
 * Sets the iid that will determine which content view to use when
 * a URI is displayed.
 * 
 * @navinfo: The NautilusNavigationInfo representing the URI that's about
 * to be displayed.
 * @fallback_value: The iid to use for the content view if no better
 * one can be determined.
 */
static void
set_initial_content_iid (NautilusNavigationInfo *navinfo,
			 const char *fallback_value)
{
	char *remembered_value = NULL;
	const char *value = NULL;

	g_assert (fallback_value != NULL);
	g_assert (g_slist_length (navinfo->content_identifiers) > 0);

	/* NOTE: Darin doesn't like the unpredictability of this three-choice system.
	 * He'd prefer a global setting and perhaps an explicit location-specific
	 * setting that doesn't affect any other locations. Maybe we should change
	 * this to work that way.
	 */
	
        remembered_value = nautilus_directory_get_metadata (navinfo->directory,
                                                            NAUTILUS_METADATA_KEY_INITIAL_VIEW,
                                                            NULL);
        
        /* Use the remembered value if it's non-NULL and in the list of choices. */
        if (remembered_value != NULL) {
                if (g_slist_find_custom (navinfo->content_identifiers,
                                         remembered_value, check_iid)) {
                        value = remembered_value;
                } else {
                        g_message ("Unknown iid \"%s\" stored for %s",
                                   remembered_value,
                                   navinfo->navinfo.requested_uri);
                }
        }
        
	if (value == NULL) {
		/* Can't use remembered value, use referring value if 
		 * it's non-NULL and in the list of choices. 
		 */
		if (navinfo->referring_iid != NULL) {
			if (g_slist_find_custom (navinfo->content_identifiers,
                                                 navinfo->referring_iid, check_iid)) {
				value = navinfo->referring_iid;
			}
		}

		/* Can't use remembered or referring value, use fallback value. */
		if (value == NULL) {
			value = fallback_value;
		}		
	}

	navinfo->initial_content_iid = g_strdup (value);

        g_free (remembered_value);
}


static char * const nautilus_sort_criteria[] = {
        /* Prefer the html view most */
        "iid == 'OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b'",
        /* Prefer the icon view next */
        "iid == 'OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058'",
        /* Prefer anything else over the sample view. */
        "iid != 'OAFIID:nautilus_sample_content_view:45c746bc-7d64-4346-90d5-6410463b43ae'",
        NULL};

/* It might be worth moving this to nautilus-string.h at some point. */
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
        return extract_prefix_add_suffix (mime_type, "/", "/*");
}

static char *
uri_string_get_scheme (const char *uri_string)
{
        return extract_prefix_add_suffix (uri_string, ":", "");
}

static char *
make_oaf_query_with_known_mime_type (NautilusNavigationInfo *navinfo)
{
        const char *mime_type;
        char *mime_supertype;
        char *uri_scheme;
        char *result;

        mime_type = navinfo->navinfo.content_type;
        mime_supertype = mime_type_get_supertype (mime_type);
        uri_scheme = uri_string_get_scheme (navinfo->navinfo.requested_uri);

        result = g_strdup_printf 
                (
                 /* Check if the component has the interfaces we need.
                  * We can work with either a Nautilus ContentView, or
                  * with a Bonobo Control or Embeddable that supports
                  * one of the three persistence interfaces:
                  * PersistStream, ProgressiveDataSink, or
                  * PersistFile.
                  */
                 "(repo_ids.has_all(['IDL:Bonobo/Control:1.0',"
                                    "'IDL:Nautilus/ContentView:1.0'])"
                  "OR (repo_ids.has_one(['IDL:Bonobo/Control:1.0',"
                                        "'IDL:Bonobo/Embeddable:1.0'])"
                      "AND repo_ids.has_one(['IDL:Bonobo/PersistStream:1.0',"
                                            "'IDL:Bonobo/ProgressiveDataSink:1.0',"
                                            "'IDL:Bonobo/PersistFile:1.0'])))"
                 
                 /* Check that the component either has a specific
                  * MIME type or URI scheme. If neither is specified,
                  * then we don't trust that to mean "all MIME types
                  * and all schemes". For that, you have to do a
                  * wildcard for the MIME type or for the scheme.
                  */
                 "AND (bonobo:supported_mime_types.defined()"
                      "OR bonobo:supported_uri_schemes.defined ())"

                 /* Check that the supported MIME types include the
                  * URI's MIME type or its supertype.
                  */
                 "AND (NOT bonobo:supported_mime_types.defined()"
                      "OR bonobo:supported_mime_types.has('%s')"
                      "OR bonobo:supported_mime_types.has('%s')"
                      "OR bonobo:supported_mime_types.has('*/*'))"

                 /* Check that the supported URI schemes include the
                  * URI's scheme.
                  */
                 "AND (NOT bonobo:supported_uri_schemes.defined()"
                      "OR bonobo:supported_uri_schemes.has('%s')"
                      "OR bonobo:supported_uri_schemes.has('*'))"

                  /* Check that the component makes it clear that it's
                   * intended for Nautilus by providing a "view_as"
                   * name. We could instead support a default, but
                   * that would make components that are untested with
                   * Nautilus appear.
                   */
                 "AND nautilus:view_as_name.defined()"

                 /* The MIME type, MIME supertype, and URI scheme for
                  * the %s above.
                  */
                 , mime_type, mime_supertype, uri_scheme);

        g_free (mime_supertype);
        g_free (uri_scheme);

        return result;
}

static char *
make_oaf_query_with_uri_scheme_only (NautilusNavigationInfo *navinfo)
{
        char *uri_scheme;
        char *result;
        
        uri_scheme = uri_string_get_scheme (navinfo->navinfo.requested_uri);

        result = g_strdup_printf 
                (
                 /* Check if the component has the interfaces we need.
                  * We can work with either a Nautilus ContentView, or
                  * with a Bonobo Control or Embeddable that works on
                  * a file, which is indicated by Bonobo PersistFile.
                  */
                  "(repo_ids.has_all(['IDL:Bonobo/Control:1.0',"
                                     "'IDL:Nautilus/ContentView:1.0'])"
                   "OR (repo_ids.has_one(['IDL:Bonobo/Control:1.0',"
                                         "'IDL:Bonobo/Embeddable:1.0'])"
                       "AND repo_ids.has('IDL:Bonobo/PersistFile:1.0')))"

                  /* Check if the component supports this particular
                   * URI scheme.
                   */
                  "AND (bonobo:supported_uri_schemes.has('%s')"
                       "OR bonobo:supported_uri_schemes.has('*'))"

                  /* Check that the component doesn't require
                   * particular MIME types. Note that even saying you support "all"
                   */
                  "AND (NOT bonobo:supported_mime_types.defined())"

                  /* Check that the component makes it clear that it's
                   * intended for Nautilus by providing a "view_as"
                   * name. We could instead support a default, but
                   * that would make components that are untested with
                   * Nautilus appear.
                   */
                  "AND nautilus:view_as_name.defined()"

                  /* The URI scheme for the %s above. */
                  , uri_scheme);

        g_free (uri_scheme);
        
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
                        mime_type = (char *) nautilus_file_get_mime_type ((NautilusFile *) p->data);
                        
                        if (NULL != mime_type) {
                                if (g_hash_table_lookup (result, mime_type) == NULL) {
#if DEBUG_MJS
                                        printf ("XXX content mime type: %s\n", mime_type);
#endif
                                        g_hash_table_insert (result, mime_type, mime_type);
                                }
                        }
                }
        }

        return result;
}


static gboolean
server_matches_content_requirements (OAF_ServerInfo *server, GHashTable *type_table)
{
        OAF_Attribute *attr;
        GNOME_stringlist types;
        int i;

        attr = oaf_server_info_attr_find (server, "nautilus:required_directory_content_mime_types");

        if (attr == NULL || attr->v._d != OAF_A_STRINGV) {
                return TRUE;
        } else {
                types = attr->v._u.value_stringv;

                for (i = 0; i < types._length; i++) {
                        if (g_hash_table_lookup (type_table, types._buffer[i]) != NULL) {
                                return TRUE;
                        }
                }
        }

        return FALSE;
}


static NautilusViewIdentifier *
nautilus_view_identifier_new_from_oaf_server_info (OAF_ServerInfo *server)
{
        const char *view_as_name;
        
        /* FIXME bugzilla.eazel.com 694: need to pass proper set of languages as 
           the last arg for i18 purposes */
        view_as_name = oaf_server_info_attr_lookup (server, "nautilus:view_as_name", NULL);

        if (view_as_name == NULL) {
                view_as_name = oaf_server_info_attr_lookup (server, "name", NULL);
        }

        if (view_as_name == NULL) {
                view_as_name = server->iid;
        }
       
        return nautilus_view_identifier_new (server->iid, view_as_name);
}

static void
got_file_info_callback (GnomeVFSAsyncHandle *ah,
                        GList *result_list,
                        gpointer data)
{
        GnomeVFSGetFileInfoResult *file_result;
        GnomeVFSResult vfs_result_code;
        NautilusNavigationInfo *navinfo;
        NautilusNavigationCallback notify_ready;
        gpointer notify_ready_data;
        NautilusNavigationResult result_code;
        const char *fallback_iid;
        const char *query;
        OAF_ServerInfoList *oaf_result;
        CORBA_Environment ev;

        g_assert (result_list != NULL);
        g_assert (result_list->data != NULL);
        g_assert (result_list->next == NULL);

        navinfo = data;

        navinfo->ah = NULL;
        
        notify_ready = navinfo->callback;
        notify_ready_data = navinfo->callback_data;

        /* Get the content type. */
        file_result = result_list->data;
        vfs_result_code = file_result->result;

        oaf_result = NULL;
        query = NULL;

        if (vfs_result_code == GNOME_VFS_OK) {
                /* FIXME bugzilla.eazel.com 697: disgusting hack to make rpm view work. Why
                   is the mime type not being detected properly in the
                   first place? */

                if (nautilus_str_has_suffix (navinfo->navinfo.requested_uri, ".rpm")) {
                        navinfo->navinfo.content_type = g_strdup ("application/x-rpm");
                } else {
                        navinfo->navinfo.content_type = g_strdup
                                (gnome_vfs_file_info_get_mime_type (file_result->file_info));
                }

                /* FIXME bugzilla.eazel.com 699: 
                   hack for lack of good type descriptions. Can
                   we remove this now? */

                if (navinfo->navinfo.content_type == NULL) {
                        navinfo->navinfo.content_type = g_strdup ("text/plain");
                }

                /* activate by scheme and mime type */

                query = make_oaf_query_with_known_mime_type (navinfo);
        } else if (vfs_result_code == GNOME_VFS_ERROR_NOTSUPPORTED
                   || vfs_result_code == GNOME_VFS_ERROR_INVALIDURI) {
                /* Activate by scheme only */

                query = make_oaf_query_with_uri_scheme_only (navinfo);
        } else {
                goto out;
        }

        CORBA_exception_init (&ev);

#ifdef DEBUG_MJS
        printf ("query: \"%s\"\n", query);
#endif

        oaf_result = oaf_query (query, nautilus_sort_criteria, &ev);
        
        if (ev._major == CORBA_NO_EXCEPTION && oaf_result != NULL && oaf_result->_length > 0) {
                GHashTable *content_types;
                int i;
                
                content_types = file_list_to_mime_type_hash_table (navinfo->files);
                
                CORBA_exception_free (&ev);
                
                vfs_result_code = GNOME_VFS_OK;
                
                for (i = 0; i < oaf_result->_length; i++) {
                        OAF_ServerInfo *server;

                        server = &oaf_result->_buffer[i];

                        if (server_matches_content_requirements (server, content_types)) {
                                navinfo->content_identifiers = g_slist_append
                                        (navinfo->content_identifiers, 
                                         nautilus_view_identifier_new_from_oaf_server_info (server));
                        }
                }

                g_hash_table_destroy (content_types);
        } else {
                CORBA_exception_free (&ev);
                result_code = NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE;
                goto out;
        }


        /* Map GnomeVFSResult to one of the types that Nautilus knows how to handle. */
        result_code = get_nautilus_navigation_result_from_gnome_vfs_result (vfs_result_code);

        if (vfs_result_code != GNOME_VFS_OK) {
                /* Leave navinfo intact so notify_ready function can access the uri.
                 * (notify_ready function is responsible for freeing navinfo).
                 */
                goto out;
        }
               
        if (navinfo->content_identifiers) {
                fallback_iid = ((NautilusViewIdentifier *)
                                (navinfo->content_identifiers->data))->iid;
#ifdef DEBUG_MJS
                printf ("XXX - fallback_iid: %s\n", fallback_iid);
#endif
        }
  
        add_components_from_metadata (navinfo);
                
        add_meta_view_iids_from_preferences (navinfo);
        
        /* Now that all the content_identifiers are in place, we're ready to choose
         * the initial one.
         */
        g_assert (fallback_iid != NULL);
        set_initial_content_iid (navinfo, fallback_iid);
        
 out:
        (* notify_ready) (result_code, navinfo, notify_ready_data);
}

/* The following routine uses metadata associated with the current url to add content view components specified in the metadata */
/* the content views are specified in the string as "label=componentname11\nlabel=componentname2\n..." */

static void
add_components_from_metadata (NautilusNavigationInfo *navinfo)
{
	char *content_views;
        char **pieces;
        const char *component_str;
        char *equal_pos;
        int index;
	
        content_views = nautilus_directory_get_metadata
                (navinfo->directory,
                 NAUTILUS_METADATA_KEY_CONTENT_VIEWS, NULL);

	if (content_views != NULL) {
	 	pieces = g_strsplit (content_views, "\n", 0);
	 	g_free (content_views); 	

	 	for (index = 0; (component_str = pieces[index]) != NULL; index++) {
			/* break the component string into the name and label */
                        puts (component_str);
                        
			equal_pos = strchr (component_str, '=');
			if (equal_pos != NULL) {
				*equal_pos++ = '\0';
				
				/* add it to the list */
				navinfo->content_identifiers = g_slist_append
                                        (navinfo->content_identifiers,
                                         nautilus_view_identifier_new (equal_pos, component_str));
			}
	 	}
	 	g_strfreev (pieces);
	}
}

static void
add_meta_view_iids_from_preferences (NautilusNavigationInfo *navinfo)
{
	const NautilusStringList *meta_view_iids;
	guint i;
        char *iid;
        gboolean enabled;
        GString	*pref_name;
        
	g_assert (navinfo != NULL);
	
	meta_view_iids = nautilus_global_preferences_get_meta_view_iids ();
	g_assert (meta_view_iids != NULL);
	
	for (i = 0; i < nautilus_string_list_get_length (meta_view_iids); i++) {
		iid = nautilus_string_list_nth (meta_view_iids, i);
		g_assert (iid != NULL);

		pref_name = g_string_new ("/nautilus/metaviews/");
		g_string_append (pref_name, iid);
		enabled = nautilus_preferences_get_boolean (pref_name->str, FALSE);
		g_string_free (pref_name, TRUE);
		
		if (enabled) {
			navinfo->meta_iids = g_slist_prepend (navinfo->meta_iids, iid);
		} else {
                        g_free (iid);
                }
	}
}

static void
got_metadata_callback (NautilusDirectory *directory,
                       GList *files,
                       gpointer callback_data)
{
        NautilusNavigationInfo *info;

        info = callback_data;
        g_assert (info->directory == directory);
        
        info->files = nautilus_file_list_copy (files);
        async_get_file_info_text (&info->ah,
                                  info->navinfo.requested_uri,
                                  (GNOME_VFS_FILE_INFO_GETMIMETYPE
                                   | GNOME_VFS_FILE_INFO_FOLLOWLINKS),
                                  got_file_info_callback,
                                  info);
}

/* NautilusNavigationInfo */

NautilusNavigationInfo *
nautilus_navigation_info_new (Nautilus_NavigationRequestInfo *nri,
                              Nautilus_NavigationInfo *old_info,
                              NautilusNavigationCallback notify_when_ready,
                              gpointer notify_data,
                              const char *referring_iid)
{
        NautilusNavigationInfo *info;
        GList *keys;
        GList *attributes;

        info = g_new0 (NautilusNavigationInfo, 1);
        
        info->callback = notify_when_ready;
        info->callback_data = notify_data;
        
        if (old_info != NULL) {
                info->navinfo.referring_uri = old_info->requested_uri;
                info->navinfo.actual_referring_uri = old_info->actual_uri;
                info->navinfo.referring_content_type = old_info->content_type;
        }

        info->referring_iid = g_strdup (referring_iid);
        info->navinfo.requested_uri = g_strdup (nri->requested_uri);

        info->directory = nautilus_directory_get (nri->requested_uri);

        /* Arrange for all the (directory) metadata we will need. */
        keys = NULL;
        keys = g_list_prepend (keys, NAUTILUS_METADATA_KEY_CONTENT_VIEWS);
        keys = g_list_prepend (keys, NAUTILUS_METADATA_KEY_INITIAL_VIEW);
        
        /* Arrange for all the file attributes we will need. */
        attributes = NULL;
        attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);
        
        nautilus_directory_call_when_ready (info->directory,
                                            keys,
                                            attributes,
                                            NULL,
                                            got_metadata_callback,
                                            info);

        g_list_free (keys);
        g_list_free (attributes);
        
        return info;
}

void
nautilus_navigation_info_cancel (NautilusNavigationInfo *info)
{
        g_return_if_fail (info != NULL);

        if (info->ah != NULL) {
                gnome_vfs_async_cancel (info->ah);
                info->ah = NULL;
        }

        nautilus_directory_cancel_callback
                (info->directory,
                 got_metadata_callback,
                 info);
}

void
nautilus_navigation_info_free (NautilusNavigationInfo *info)
{
        g_return_if_fail (info != NULL);
        
        nautilus_navigation_info_cancel (info);

        g_slist_foreach (info->content_identifiers,
                         (GFunc) nautilus_view_identifier_free,
                         NULL);
        g_slist_free (info->content_identifiers);

        g_slist_foreach (info->meta_iids, (GFunc) g_free, NULL);
        g_slist_free (info->meta_iids);

        g_free (info->referring_iid);
        g_free (info->initial_content_iid);
        g_free (info->navinfo.requested_uri);
        g_free (info->navinfo.actual_uri);
        g_free (info->navinfo.content_type);

        nautilus_directory_unref (info->directory);
        nautilus_file_list_free (info->files);

        g_free (info);
}

/* Cover for getting file info for one file. */
static void
async_get_file_info_text (GnomeVFSAsyncHandle **handle,
                          const char *text_uri,
                          GnomeVFSFileInfoOptions options,
                          GnomeVFSAsyncGetFileInfoCallback callback,
                          gpointer callback_data)
{
        GnomeVFSURI *vfs_uri;
        GList uri_list;
        GList result_list;
        GnomeVFSGetFileInfoResult result_item;
        
        vfs_uri = gnome_vfs_uri_new (text_uri);
        if (vfs_uri == NULL) {
                /* Report the error. */

                *handle = NULL;
                
                result_item.uri = NULL;
                result_item.result = GNOME_VFS_ERROR_INVALIDURI;
                result_item.file_info = NULL;
                
                result_list.data = &result_item;
                result_list.next = NULL;
                
                (* callback) (NULL, &result_list, callback_data);
                
                return;
        }

        /* Construct a simple URI list. */
        uri_list.data = vfs_uri;
        uri_list.next = NULL;

        gnome_vfs_async_get_file_info (handle,
                                       &uri_list,
                                       options,
                                       NULL,
                                       callback,
                                       callback_data);
        
        gnome_vfs_uri_unref (vfs_uri);
}
