/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Maciej Stachowiak <mjs@eazel.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

/* nautilus-applicable-views.c: Implementation of routines for mapping a location
   change request to a set of views and actual URL to be loaded. */

#include <config.h>
#include "nautilus-applicable-views.h"

#include <libgnomevfs/gnome-vfs-result.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-view-identifier.h>

struct NautilusDetermineViewHandle {
        NautilusDetermineViewHandle **early_completion_hook;
	NautilusDetermineViewCallback callback;
	gpointer callback_data;
        NautilusFile *file;
};

static NautilusDetermineViewResult
get_view_result_from_gnome_vfs_result (GnomeVFSResult gnome_vfs_result)
{
        switch (gnome_vfs_result) {
        case GNOME_VFS_OK:
                return NAUTILUS_DETERMINE_VIEW_OK;
        case GNOME_VFS_ERROR_NOT_FOUND:
                return NAUTILUS_DETERMINE_VIEW_NOT_FOUND;
        case GNOME_VFS_ERROR_INVALID_URI:
                return NAUTILUS_DETERMINE_VIEW_INVALID_URI;
        case GNOME_VFS_ERROR_NOT_SUPPORTED:
                return NAUTILUS_DETERMINE_VIEW_UNSUPPORTED_SCHEME;
	case GNOME_VFS_ERROR_LOGIN_FAILED:
		return NAUTILUS_DETERMINE_VIEW_LOGIN_FAILED;
	case GNOME_VFS_ERROR_SERVICE_NOT_AVAILABLE:	
		return NAUTILUS_DETERMINE_VIEW_SERVICE_NOT_AVAILABLE;
	case GNOME_VFS_ERROR_ACCESS_DENIED:	
		return NAUTILUS_DETERMINE_VIEW_ACCESS_DENIED;
        case GNOME_VFS_ERROR_HOST_NOT_FOUND:
                return NAUTILUS_DETERMINE_VIEW_HOST_NOT_FOUND;
	case GNOME_VFS_ERROR_HOST_HAS_NO_ADDRESS:
		return NAUTILUS_DETERMINE_VIEW_HOST_HAS_NO_ADDRESS;
        case GNOME_VFS_ERROR_GENERIC:
        case GNOME_VFS_ERROR_INTERNAL:
                /* These two have occurred at least once in the web browser component */
                return NAUTILUS_DETERMINE_VIEW_UNSPECIFIC_ERROR;
        default:
                /* Whenever this message fires, we should consider adding a specific case
                 * to make the error as comprehensible as possible to the user. Please
                 * bug me (sullivan@eazel.com) if you see this fire and don't have the
                 * inclination to immediately make a good message yourself (tell me
                 * what GnomeVFSResult code the message reported, and what caused it to
                 * fire).
                 */
                g_warning ("in nautilus-applicable-views.c, got unhandled GnomeVFSResult %d (%s). "
                           "If this is a legitimate get_file_info result, please tell "
                           "sullivan@eazel.com so he can "
                	   "write a decent user-level error message for it.", 
                	   gnome_vfs_result,
                	   gnome_vfs_result_to_string (gnome_vfs_result));
                return NAUTILUS_DETERMINE_VIEW_UNSPECIFIC_ERROR;
        }
}

static void
got_file_info_callback (NautilusFile *file,
                        gpointer callback_data)
{
        NautilusDetermineViewHandle *handle;
        GnomeVFSResult vfs_result_code;
        NautilusDetermineViewResult result_code;
        NautilusViewIdentifier *default_id;
        OAF_ServerInfo *default_component;
        
        handle = (NautilusDetermineViewHandle *) callback_data;
        
        g_assert (handle->file == file);
	default_id = NULL;
        
        vfs_result_code = nautilus_file_get_file_info_result (file);
        if (vfs_result_code == GNOME_VFS_OK
            || vfs_result_code == GNOME_VFS_ERROR_NOT_SUPPORTED
            || vfs_result_code == GNOME_VFS_ERROR_INVALID_URI) {
                default_component = nautilus_mime_get_default_component_for_file (handle->file);
                if (default_component != NULL) {
                        default_id = nautilus_view_identifier_new_from_content_view (default_component);
                        CORBA_free (default_component);
                        if (default_id != NULL) {
                                vfs_result_code = GNOME_VFS_OK;
                        }
                }
        }
        
        if (vfs_result_code == GNOME_VFS_OK && default_id == NULL) {
                /* If the complete list is non-empty, the default shouldn't have been NULL */
                g_assert (!nautilus_mime_has_any_components_for_file (handle->file));
                result_code = NAUTILUS_DETERMINE_VIEW_NO_HANDLER_FOR_TYPE;
        } else {
                result_code = get_view_result_from_gnome_vfs_result (vfs_result_code);
 	}

        (* handle->callback) (handle,
                              result_code,
                              default_id,
                              handle->callback_data);
        
        nautilus_view_identifier_free (default_id);

        nautilus_determine_initial_view_cancel (handle);
}

NautilusDetermineViewHandle *
nautilus_determine_initial_view (const char *location,
                                 NautilusDetermineViewCallback callback,
                                 gpointer callback_data)
{
        NautilusDetermineViewHandle *handle;
        GList *attributes;

        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (callback != NULL, NULL);

        handle = g_new0 (NautilusDetermineViewHandle, 1);
        
        handle->early_completion_hook = &handle;

        handle->callback = callback;
        handle->callback_data = callback_data;
        
        handle->file = nautilus_file_get (location);

        attributes = nautilus_mime_actions_get_minimum_file_attributes ();
        nautilus_file_call_when_ready (handle->file, attributes,
                                       got_file_info_callback, handle);
        g_list_free (attributes);

        if (handle != NULL) {
                handle->early_completion_hook = NULL;
        }
        
        return handle;
}

void
nautilus_determine_initial_view_cancel (NautilusDetermineViewHandle *handle)
{
        g_return_if_fail (handle != NULL);
        g_return_if_fail (handle->early_completion_hook == NULL
                          || *handle->early_completion_hook == handle);
        g_return_if_fail (handle->callback != NULL);

        if (handle->early_completion_hook != NULL) {
                *handle->early_completion_hook = NULL;
        }

        nautilus_file_cancel_call_when_ready
                (handle->file, got_file_info_callback, handle);

        nautilus_file_unref (handle->file);

        g_free (handle);
}
