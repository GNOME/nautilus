/*
 * nautilus-column-provider.h - Interface for Nautilus extensions that provide column descriptions.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
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

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COLUMN_PROVIDER (nautilus_column_provider_get_type ())

G_DECLARE_INTERFACE (NautilusColumnProvider, nautilus_column_provider,
                     NAUTILUS, COLUMN_PROVIDER,
                     GObject)

struct _NautilusColumnProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_columns) (NautilusColumnProvider *provider);
};

GList *nautilus_column_provider_get_columns (NautilusColumnProvider *provider);

G_END_DECLS
