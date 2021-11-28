/*
 *  nautilus-column-provider.h - Interface for Nautilus extensions that 
 *                               provide column descriptions.
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
 * add columns to the list view and details to the icon view.
 * Extensions are asked for a list of columns to display.  Each
 * returned column refers to a string attribute which can be filled in
 * by NautilusInfoProvider */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
/* These should be removed at some point. */
#include "nautilus-extension-types.h"
#include "nautilus-column.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COLUMN_PROVIDER (nautilus_column_provider_get_type ())

G_DECLARE_INTERFACE (NautilusColumnProvider, nautilus_column_provider,
                     NAUTILUS, COLUMN_PROVIDER,
                     GObject)

/* For compatibility reasons, remove this once you start introducing breaking changes. */
/**
 * NautilusColumnProviderIface: (skip)
 */
typedef NautilusColumnProviderInterface NautilusColumnProviderIface;

/**
 * SECTION:nautilus-column-provider
 * @title: NautilusColumnProvider
 * @short_description: Interface to provide additional list view columns
 *
 * #NautilusColumnProvider allows extension to provide additional columns
 * in the file manager list view.
 */

/**
 * NautilusColumnProviderInterface:
 * @g_iface: The parent interface.
 * @get_columns: Returns a #GList of #NautilusColumn.
 *               See nautilus_column_provider_get_columns() for details.
 *
 * Interface for extensions to provide additional list view columns.
 */
struct _NautilusColumnProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_columns) (NautilusColumnProvider *provider);
};

/**
 * nautilus_column_provider_get_columns:
 * @provider: a #NautilusColumnProvider
 *
 * Returns: (element-type NautilusColumn) (transfer full): the provided #NautilusColumn objects
 */
GList *nautilus_column_provider_get_columns (NautilusColumnProvider *provider);

G_END_DECLS
