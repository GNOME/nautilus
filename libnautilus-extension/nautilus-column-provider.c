/*
 *  nautilus-column-provider.c - Interface for Nautilus extensions
 *                               that provide column specifications.
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

#include "nautilus-column-provider.h"

/**
 * NautilusColumnProvider:
 *
 * Interface to provide additional list view columns
 *
 * Allows extension to provide additional columns in the file manager list view.
 */

G_DEFINE_INTERFACE (NautilusColumnProvider, nautilus_column_provider, G_TYPE_OBJECT)

static void
nautilus_column_provider_default_init (NautilusColumnProviderInterface *klass)
{
}

/**
 * nautilus_column_provider_get_columns:
 *
 * Provide a list of [class@Column].
 *
 * The `NautilusColumnProvider` only provides the metadata of the columns
 * (importantly the [property@Column:attribute]).  You will very likely also
 * implement [iface@InfoProvider] in order to call [method@FileInfo.add_string_attribute]
 * on all necessary files.
 *
 * This method should return immediately without any blocking i/o.
 *
 * Returns: (nullable) (element-type NautilusColumn) (transfer full): the provided `NautilusColumn` objects
 */
GList *
nautilus_column_provider_get_columns (NautilusColumnProvider *self)
{
    NautilusColumnProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_COLUMN_PROVIDER (self), NULL);

    iface = NAUTILUS_COLUMN_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_columns != NULL, NULL);

    return iface->get_columns (self);
}
