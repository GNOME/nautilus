/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
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


/* #define DEBUG_MJS 1 */

/* nautilus-applicable-views.c: Implementation of routines for mapping a location
   change request to a set of views and actual URL to be loaded. */

#include <config.h>
#include "nautilus-applicable-views.h"

#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-mime-actions.h>

#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>

#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>

/* forward declarations */
static void async_get_file_info_text            (GnomeVFSAsyncHandle              **handle,
                                                 const char                        *text_uri,
                                                 GnomeVFSFileInfoOptions            options,
                                                 GnomeVFSAsyncGetFileInfoCallback   callback,
                                                 gpointer                           callback_data);

static NautilusNavigationResult
get_nautilus_navigation_result_from_gnome_vfs_result (GnomeVFSResult gnome_vfs_result)
{
        switch (gnome_vfs_result) {
        case GNOME_VFS_OK:
                return NAUTILUS_NAVIGATION_RESULT_OK;
        case GNOME_VFS_ERROR_NOT_FOUND:
        case GNOME_VFS_ERROR_HOST_NOT_FOUND:
                return NAUTILUS_NAVIGATION_RESULT_NOT_FOUND;
        case GNOME_VFS_ERROR_INVALID_URI:
                return NAUTILUS_NAVIGATION_RESULT_INVALID_URI;
        case GNOME_VFS_ERROR_NOT_SUPPORTED:
                return NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME;
	case GNOME_VFS_ERROR_LOGIN_FAILED:
		return NAUTILUS_NAVIGATION_RESULT_LOGIN_FAILED;
	case GNOME_VFS_ERROR_SERVICE_NOT_AVAILABLE:	
		return NAUTILUS_NAVIGATION_RESULT_SERVICE_NOT_AVAILABLE;
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
                g_message ("in nautilus-applicable-views.c, got unhandled GnomeVFSResult %d, please tell sullivan@eazel.com", gnome_vfs_result);
                return NAUTILUS_NAVIGATION_RESULT_UNSPECIFIC_ERROR;
        }
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
        NautilusViewIdentifier *default_id;
        OAF_ServerInfo *default_component;

        g_assert (result_list != NULL);
        g_assert (result_list->data != NULL);
        g_assert (result_list->next == NULL);

        result_code = NAUTILUS_NAVIGATION_RESULT_UNDEFINED;
	default_id = NULL;

        navinfo = data;

        navinfo->ah = NULL;
        
        notify_ready = navinfo->callback;
        notify_ready_data = navinfo->callback_data;

        /* Get the content type. */
        file_result = result_list->data;
        vfs_result_code = file_result->result;

        if (vfs_result_code != GNOME_VFS_OK
            && vfs_result_code != GNOME_VFS_ERROR_NOT_SUPPORTED
            && vfs_result_code != GNOME_VFS_ERROR_INVALID_URI) {
                goto out;
        }

        default_component = nautilus_mime_get_default_component_for_uri (navinfo->location);
        if (default_component != NULL) {
        	default_id = nautilus_view_identifier_new_from_content_view (default_component);
                CORBA_free (default_component);
        }
        
#ifdef DEBUG_MJS
        printf ("XXXXXX - default_id: %s (%s)\n", default_id->iid, default_id->name);
#endif

        /* If no components found at all - if there are any, there will be a default. */
        if (default_id != NULL) {
                vfs_result_code = GNOME_VFS_OK;
                result_code = get_nautilus_navigation_result_from_gnome_vfs_result (vfs_result_code);
        } else {
                /* Map GnomeVFSResult to one of the types that Nautilus knows how to handle. */
                if (vfs_result_code == GNOME_VFS_OK && default_id == NULL) {
                	/* If the complete list is non-empty, the default shouldn't have been NULL */
                    	g_assert (!nautilus_mime_has_any_components_for_uri (navinfo->location));
                        result_code = NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE;
                }

		/* As long as we have any usable id, we can view this location. */
                if (default_id == NULL) {
	                goto out;
                }
        }
               
        g_assert (default_id != NULL);
	navinfo->initial_content_id = nautilus_view_identifier_copy (default_id);
        
 out:
 	if (result_code == NAUTILUS_NAVIGATION_RESULT_UNDEFINED) {
                result_code = get_nautilus_navigation_result_from_gnome_vfs_result (vfs_result_code);
 	}
        (* notify_ready) (result_code, navinfo, notify_ready_data);
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
                                  info->location,
                                  (GNOME_VFS_FILE_INFO_GET_MIME_TYPE
                                   | GNOME_VFS_FILE_INFO_FOLLOW_LINKS),
                                  got_file_info_callback,
                                  info);
}

/* NautilusNavigationInfo */

NautilusNavigationInfo *
nautilus_navigation_info_new (const char *location,
                              NautilusNavigationCallback notify_when_ready,
                              gpointer notify_data,
                              const char *referring_iid)
{
        NautilusNavigationInfo *info;
        GList *attributes;

        info = g_new0 (NautilusNavigationInfo, 1);
        
        info->callback = notify_when_ready;
        info->callback_data = notify_data;
        
        info->referring_iid = g_strdup (referring_iid);
        info->location = g_strdup (location);

        info->directory = nautilus_directory_get (location);

        /* Arrange for all the file attributes we will need. */
        attributes = NULL;
        attributes = g_list_prepend (attributes, NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);
        
        nautilus_directory_call_when_ready
                (info->directory,
                 attributes, TRUE,
                 got_metadata_callback, info);
        
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

        nautilus_g_list_free_deep (info->explicit_iids);

        nautilus_view_identifier_free (info->initial_content_id);
        g_free (info->referring_iid);
        g_free (info->location);

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
                result_item.result = GNOME_VFS_ERROR_INVALID_URI;
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
                                       callback,
                                       callback_data);
        
        gnome_vfs_uri_unref (vfs_uri);
}
