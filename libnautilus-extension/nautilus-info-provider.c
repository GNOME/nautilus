/*
 *  nautilus-info-provider.c - Interface for Nautilus extensions that
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

#include "nautilus-info-provider.h"

#include "nautilus-extension-enum-types.h"

/**
 * NautilusInfoProvider:
 *
 * Interface to provide additional information about files
 *
 * `NautilusInfoProvider` allows extension to provide additional information about
 * files. When [method@InfoProvider.update_file_info] is called by the application,
 * extensions will know that it's time to add extra information to the provided
 * [iface@FileInfo].
 */

G_DEFINE_INTERFACE (NautilusInfoProvider, nautilus_info_provider, G_TYPE_OBJECT)

static void
nautilus_info_provider_default_init (NautilusInfoProviderInterface *klass)
{
}

/**
 * nautilus_info_provider_update_file_info:
 * @file: a file requesting updated info.
 * @update_complete: the closure to invoke at some later time when returning
 *                   @NAUTILUS_OPERATION_IN_PROGRESS.
 * @handle: (transfer none) (nullable) (out): an opaque #NautilusOperationHandle
 *           that must be set when returning @NAUTILUS_OPERATION_IN_PROGRESS.
 *
 * Called when the Nautilus application is requesting updated file information.
 *
 * Returns: whether the operation finished.
 */
NautilusOperationResult
nautilus_info_provider_update_file_info (NautilusInfoProvider     *self,
                                         NautilusFileInfo         *file,
                                         GClosure                 *update_complete,
                                         NautilusOperationHandle **handle)
{
    NautilusInfoProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_INFO_PROVIDER (self),
                          NAUTILUS_OPERATION_FAILED);
    g_return_val_if_fail (update_complete != NULL,
                          NAUTILUS_OPERATION_FAILED);
    g_return_val_if_fail (handle != NULL, NAUTILUS_OPERATION_FAILED);

    iface = NAUTILUS_INFO_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->update_file_info != NULL,
                          NAUTILUS_OPERATION_FAILED);

    return iface->update_file_info (self, file, update_complete, handle);
}

/**
 * nautilus_info_provider_cancel_update:
 * @provider: a #NautilusInfoProvider
 * @handle: the opaque #NautilusOperationHandle returned from a previous call to
 *          nautilus_info_provider_update_file_info().
 *
 * Called when the Nautilus application has canceled an update.
 *
 * This method is only relevant if [method@InfoProvider.update_file_info] was returned with
 * `NAUTILUS_OPERATION_IN_PROGRESS`.
 */
void
nautilus_info_provider_cancel_update (NautilusInfoProvider    *self,
                                      NautilusOperationHandle *handle)
{
    NautilusInfoProviderInterface *iface;

    g_return_if_fail (NAUTILUS_IS_INFO_PROVIDER (self));
    g_return_if_fail (handle != NULL);

    iface = NAUTILUS_INFO_PROVIDER_GET_IFACE (self);

    g_return_if_fail (iface->cancel_update != NULL);

    iface->cancel_update (self, handle);
}

/**
 * nautilus_info_provider_update_complete_invoke:
 * @update_complete: the closure to call
 * @provider: an info provider
 * @handle: the handle for the given closure
 * @result: either `NAUTILUS_OPERATION_COMPLETE` or `NAUTILUS_OPERATION_FAILED`
 *
 * Complete an async file info update.
 */
void
nautilus_info_provider_update_complete_invoke (GClosure                *update_complete,
                                               NautilusInfoProvider    *provider,
                                               NautilusOperationHandle *handle,
                                               NautilusOperationResult  result)
{
    GValue args[3] = { { 0, } };
    GValue return_val = { 0, };

    g_return_if_fail (update_complete != NULL);
    g_return_if_fail (NAUTILUS_IS_INFO_PROVIDER (provider));

    g_value_init (&args[0], NAUTILUS_TYPE_INFO_PROVIDER);
    g_value_init (&args[1], G_TYPE_POINTER);
    g_value_init (&args[2], NAUTILUS_TYPE_OPERATION_RESULT);

    g_value_set_object (&args[0], provider);
    g_value_set_pointer (&args[1], handle);
    g_value_set_enum (&args[2], result);

    g_closure_invoke (update_complete, &return_val, 3, args, NULL);

    g_value_unset (&args[0]);
    g_value_unset (&args[1]);
    g_value_unset (&args[2]);
}
