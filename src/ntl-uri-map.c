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

#include "ntl-uri-map.h"

#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-string.h>

#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "ntl-types.h"

/* forward declarations */

static void add_components_from_metadata        (NautilusNavigationInfo *navinfo);
static void add_meta_view_iids_from_preferences (NautilusNavigationInfo *navinfo);

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

static void
my_notify_when_ready (GnomeVFSAsyncHandle *ah,
                      GnomeVFSResult result,
                      GnomeVFSFileInfo *vfs_fileinfo,
                      gpointer data)
{
        NautilusNavigationInfo *navinfo;
        NautilusNavigationCallback notify_ready;
        gpointer notify_ready_data;
        const char *fallback_iid;
        NautilusNavigationResult result_code;

        navinfo = data;

        navinfo->ah = NULL;
        
        notify_ready = navinfo->callback;
        notify_ready_data = navinfo->callback_data;

        /* Get the content type. */
        if (result == GNOME_VFS_OK) {
                navinfo->navinfo.content_type = g_strdup (gnome_vfs_file_info_get_mime_type (vfs_fileinfo));
        } else if (result == GNOME_VFS_ERROR_NOTSUPPORTED
                   || result == GNOME_VFS_ERROR_INVALIDURI) {
                /* Special scheme mapping stuff */
                if (nautilus_str_has_prefix (navinfo->navinfo.requested_uri, "irc://")) {
                        navinfo->navinfo.content_type = g_strdup ("special/x-irc-session");
                        result = GNOME_VFS_OK;
                } else if (nautilus_str_has_prefix (navinfo->navinfo.requested_uri, "eazel:")) {
                        navinfo->navinfo.content_type = g_strdup ("special/eazel-service");
                        result = GNOME_VFS_OK;
                } else if (nautilus_str_has_prefix (navinfo->navinfo.requested_uri, "hardware:")) {
                        navinfo->navinfo.content_type = g_strdup ("special/hardware");
                        result = GNOME_VFS_OK;
                /* FIXME: This mozilla-hack should be short lived until http issues are solved */
                } else if (nautilus_str_has_prefix (navinfo->navinfo.requested_uri, "moz:")) {
                        navinfo->navinfo.content_type = g_strdup ("special/mozilla-hack");
                        result = GNOME_VFS_OK;
                }
        }
                        
        /* Map GnomeVFSResult to one of the types that Nautilus knows how to handle. */
        result_code = get_nautilus_navigation_result_from_gnome_vfs_result (result);
  
        if (result != GNOME_VFS_OK) {
                /* Leave navinfo intact so notify_ready function can access the uri.
                 * (notify_ready function is responsible for freeing navinfo).
                 */
                goto out;
        }

        /* Given a content type and a URI, what do we do? Basically the "expert system" below
           tries to answer that question
           
           Check if the URI is in an abnormal scheme (e.g. one not supported by gnome-vfs)
           If so
             Lookup content views by scheme name, go.
             Lookup meta views by scheme name, go.
           
           If not
             Figure out content type.
             Lookup content views by content type, go.
             Lookup meta views by content type, go.
           
           The lookup-and-go process works like:
             Generate a list of all possibilities ordered by quality.
             Put possibilities on menu.
           
             Find if the user has specified any default(s) globally, modify selection.
             Find if the user has specified any default(s) per-page, modify selection.
        */
        
        /* This is just a hardcoded hack until OAF works with Bonobo.
           In the future we will use OAF queries to determine this information. */
        
        if (navinfo->navinfo.content_type == NULL) {
                navinfo->navinfo.content_type = g_strdup ("text/plain");
        }
        
        if (strcmp (navinfo->navinfo.content_type, "text/html") == 0) {
                fallback_iid = "OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b", "Web Page"));
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:bonobo_text-plain:26e1f6ba-90dd-4783-b304-6122c4b6c821", "Text"));
        } else if (nautilus_str_has_prefix (navinfo->navinfo.content_type, "image/")) {
                fallback_iid = "eog-image-viewer";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("eog-image-viewer", "Image"));
        } else if (strcmp (navinfo->navinfo.content_type, "special/x-irc-session") == 0) {
                fallback_iid = "xchat";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("xchat", "Chat room"));
        } else if (strcmp(navinfo->navinfo.content_type, "special/directory") == 0
                   || strcmp(navinfo->navinfo.content_type, "application/x-nautilus-vdir") == 0) {
                fallback_iid = "OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058", "Icons"));
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:ntl_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c", "List"));
                
                /* besides the information in OAF/GConf, we also want to offer components that are specifically refered to in the metadata,
                   so we ask the metadata for content views here and add them accordingly.  */      
                
                /* FIXME:  for now, we just do this for directories but it should apply to all places with available metadata */
                add_components_from_metadata (navinfo);
        } else if (strcmp (navinfo->navinfo.content_type, "special/webdav-directory") == 0) {
                fallback_iid = "OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:ntl_web_browser:0ce1a736-c939-4ac7-b12c-19d72bf1510b", "Web Page"));
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:ntl_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058", "Icons"));
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:ntl_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c", "List"));
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:bonobo_text-plain:26e1f6ba-90dd-4783-b304-6122c4b6c821", "Text"));
                
                /* besides the information in OAF/GConf, we also want to offer components that are specifically refered to in the metadata,
                   so we ask the metadata for content views here and add them accordingly.  */      
	   
                /* FIXME:  for now, we just do this for directories but it should apply to all places with available metadata */
                add_components_from_metadata (navinfo);
        }
        else if (strcmp (navinfo->navinfo.content_type, "application/x-rpm") == 0
                 || nautilus_str_has_suffix (navinfo->navinfo.requested_uri, ".rpm")) {
                fallback_iid = "OAFIID:nautilus_rpm_view:22ea002c-11e6-44fd-b13c-9445175a5e70";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:nautilus_rpm_view:22ea002c-11e6-44fd-b13c-9445175a5e70", 
                                                       "Package"));      
        } else if (strcmp(navinfo->navinfo.content_type, "special/eazel-service") == 0) {
                fallback_iid = "OAFIID:nautilus_service_startup_view:a8f1b0ef-a39f-4f92-84bc-1704f0321a82";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:nautilus_service_startup_view:a8f1b0ef-a39f-4f92-84bc-1704f0321a82", "Service"));      
        } else if (strcmp(navinfo->navinfo.content_type, "special/hardware") == 0) {
                fallback_iid = "OAFIID:nautilus_hardware_view:20000422-2250";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:nautilus_hardware_view:20000422-2250", "Hardware"));      
        /* FIXME: This mozilla-hack should be short lived until http issues are solved */
        } else if (strcmp(navinfo->navinfo.content_type, "special/mozilla-hack") == 0) {
                fallback_iid = "OAFIID:nautilus_mozilla_content_view:1ee70717-57bf-4079-aae5-922abdd576b1";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:nautilus_mozilla_content_view:1ee70717-57bf-4079-aae5-922abdd576b1",
                                                       "Mozilla"));
        } else if (strcmp(navinfo->navinfo.content_type, "text/plain") == 0) {
                fallback_iid = "OAFIID:bonobo_text-plain:26e1f6ba-90dd-4783-b304-6122c4b6c821";
                navinfo->content_identifiers = g_slist_append
                        (navinfo->content_identifiers, 
                         nautilus_view_identifier_new ("OAFIID:bonobo_text-plain:26e1f6ba-90dd-4783-b304-6122c4b6c821", "Text"));
        } else {
                /* Can't display file; nothing registered to handle this file type. */
                result_code = NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE;
                goto out;
        }
        
        /* FIXME: Should do this only when in some special testing mode or something. */
        navinfo->content_identifiers = g_slist_append
                (navinfo->content_identifiers, 
                 nautilus_view_identifier_new ("OAFIID:nautilus_sample_content_view:45c746bd-7d64-4346-90d5-6410463b43ae", "Sample"));
        
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
		enabled = nautilus_preferences_get_boolean (nautilus_preferences_get_global_preferences (),
							    pref_name->str,
                                                            TRUE);
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
                       gpointer callback_data)
{
        NautilusNavigationInfo *info;

        info = callback_data;
        g_assert (info->directory == directory);
        
        gnome_vfs_async_get_file_info (&info->ah,
                                       info->navinfo.requested_uri,
                                       (GNOME_VFS_FILE_INFO_GETMIMETYPE
                                        | GNOME_VFS_FILE_INFO_FOLLOWLINKS),
                                       NULL,
                                       my_notify_when_ready,
                                       info);
}

/* NautilusNavigationInfo */

NautilusNavigationInfo *
nautilus_navigation_info_new (Nautilus_NavigationRequestInfo *nri,
                              Nautilus_NavigationInfo *old_navinfo,
                              NautilusNavigationCallback notify_when_ready,
                              gpointer notify_data,
                              const char *referring_iid)
{
        NautilusNavigationInfo *navinfo;
        GList *keys;
        
        navinfo = g_new0 (NautilusNavigationInfo, 1);
        
        navinfo->callback = notify_when_ready;
        navinfo->callback_data = notify_data;
        
        if (old_navinfo != NULL) {
                navinfo->navinfo.referring_uri = old_navinfo->requested_uri;
                navinfo->navinfo.actual_referring_uri = old_navinfo->actual_uri;
                navinfo->navinfo.referring_content_type = old_navinfo->content_type;
        }

        navinfo->referring_iid = g_strdup (referring_iid);
        navinfo->navinfo.requested_uri = g_strdup (nri->requested_uri);

        navinfo->directory = nautilus_directory_get (nri->requested_uri);

        /* Arrange for all the metadata we will need. */
        keys = NULL;
        keys = g_list_prepend (keys, NAUTILUS_METADATA_KEY_CONTENT_VIEWS);
        keys = g_list_prepend (keys, NAUTILUS_METADATA_KEY_INITIAL_VIEW);
        nautilus_directory_call_when_ready (navinfo->directory,
                                            keys,
                                            NULL,
                                            got_metadata_callback,
                                            navinfo);
        g_list_free (keys);
        
        return navinfo;
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
                (info->directory, got_metadata_callback, info);
}

void
nautilus_navigation_info_free (NautilusNavigationInfo *navinfo)
{
        g_return_if_fail (navinfo != NULL);
        
        nautilus_navigation_info_cancel (navinfo);

        g_slist_foreach (navinfo->content_identifiers, (GFunc) nautilus_view_identifier_free, NULL);
        g_slist_free (navinfo->content_identifiers);
        g_slist_foreach (navinfo->meta_iids, (GFunc) g_free, NULL);
        g_slist_free (navinfo->meta_iids);
        g_free (navinfo->referring_iid);
        g_free (navinfo->initial_content_iid);
        g_free (navinfo->navinfo.requested_uri);
        g_free (navinfo->navinfo.actual_uri);
        g_free (navinfo->navinfo.content_type);
        nautilus_directory_unref (navinfo->directory);
        g_free (navinfo);
}
