/*
 * nautilus-info-provider.h - Interface for Nautilus extensions that provide info about files.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
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
 * NautilusOperationHandle:
 *
 * Handle for asynchronous interfaces.
 *
 * These are opaque handles that must be unique within an extension object.
 * These are set by operations that return `NAUTILUS_OPERATION_IN_PROGRESS`.
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
 */
typedef enum
{
    NAUTILUS_OPERATION_COMPLETE,
    NAUTILUS_OPERATION_FAILED,
    NAUTILUS_OPERATION_IN_PROGRESS
} NautilusOperationResult;

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

NautilusOperationResult nautilus_info_provider_update_file_info       (NautilusInfoProvider     *provider,
                                                                       NautilusFileInfo         *file,
                                                                       GClosure                 *update_complete,
                                                                       NautilusOperationHandle **handle);

void                    nautilus_info_provider_cancel_update          (NautilusInfoProvider     *provider,
                                                                       NautilusOperationHandle  *handle);

void                    nautilus_info_provider_update_complete_invoke (GClosure                 *update_complete,
                                                                       NautilusInfoProvider     *provider,
                                                                       NautilusOperationHandle  *handle,
                                                                       NautilusOperationResult   result);

G_END_DECLS
