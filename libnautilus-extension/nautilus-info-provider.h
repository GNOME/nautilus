/*
 *  nautilus-info-provider.h - Interface for Nautilus extensions that 
 *                             provide info about files.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

/* This interface is implemented by Nautilus extensions that want to 
 * provide information about files.  Extensions are called when Nautilus 
 * needs information about a file.  They are passed a NautilusFileInfo 
 * object which should be filled with relevant information */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include "nautilus-file-info.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_INFO_PROVIDER (nautilus_info_provider_get_type ())

G_DECLARE_INTERFACE (NautilusInfoProvider, nautilus_info_provider,
                     NAUTILUS, INFO_PROVIDER,
                     GObject)

/**
 * SECTION:nautilus-info-provider
 * @title: NautilusInfoProvider
 * @short_description: Interface to provide additional information about files
 *
 * #NautilusInfoProvider allows extension to provide additional information about
 * files. When nautilus_info_provider_update_file_info() is called by the application,
 * extensions will know that it's time to add extra information to the provided
 * #NautilusFileInfo.
 */

/**
 * NautilusOperationHandle:
 *
 * Handle for asynchronous interfaces. These are opaque handles that must
 * be unique within an extension object. These are returned by operations
 * that return #NAUTILUS_OPERATION_IN_PROGRESS.
 */
typedef struct _NautilusOperationHandle NautilusOperationHandle;

/**
 * NautilusOperationResult:
 * @NAUTILUS_OPERATION_COMPLETE: the operation succeeded, and the extension
 *  is done with the request.
 * @NAUTILUS_OPERATION_FAILED: the operation failed.
 * @NAUTILUS_OPERATION_IN_PROGRESS: the extension has begin an async operation.
 *  When this value is returned, the extension must set the handle parameter
 *  and call the callback closure when the operation is complete.
 *
 * Return values for asynchronous operations performed by the extension.
 * See nautilus_info_provider_update_file_info().
 */
typedef enum
{
    /* Returned if the call succeeded, and the extension is done 
     * with the request */
    NAUTILUS_OPERATION_COMPLETE,

    /* Returned if the call failed */
    NAUTILUS_OPERATION_FAILED,

    /* Returned if the extension has begun an async operation. 
     * If this is returned, the extension must set the handle 
     * parameter and call the callback closure when the 
     * operation is complete. */
    NAUTILUS_OPERATION_IN_PROGRESS
} NautilusOperationResult;

/**
 * NautilusInfoProviderInterface:
 * @g_iface: The parent interface.
 * @update_file_info: Returns a #NautilusOperationResult.
 *                    See nautilus_info_provider_update_file_info() for details.
 * @cancel_update: Cancels a previous call to nautilus_info_provider_update_file_info().
 *                 See nautilus_info_provider_cancel_update() for details.
 *
 * Interface for extensions to provide additional information about files.
 */
struct _NautilusInfoProviderInterface
{
    GTypeInterface g_iface;

    NautilusOperationResult (*update_file_info) (NautilusInfoProvider     *provider,
                                                 NautilusFileInfo         *file,
                                                 GClosure                 *update_complete,
                                                 NautilusOperationHandle **handle);
    void                    (*cancel_update)    (NautilusInfoProvider     *provider,
                                                 NautilusOperationHandle  *handle);
};

/* Interface Functions */
NautilusOperationResult nautilus_info_provider_update_file_info       (NautilusInfoProvider     *provider,
                                                                       NautilusFileInfo         *file,
                                                                       GClosure                 *update_complete,
                                                                       NautilusOperationHandle **handle);
void                    nautilus_info_provider_cancel_update          (NautilusInfoProvider     *provider,
                                                                       NautilusOperationHandle  *handle);



/* Helper functions for implementations */
void                    nautilus_info_provider_update_complete_invoke (GClosure                 *update_complete,
                                                                       NautilusInfoProvider     *provider,
                                                                       NautilusOperationHandle  *handle,
                                                                       NautilusOperationResult   result);

G_END_DECLS
