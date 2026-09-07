/*
 * nautilus-menu-provider.h - Interface for Nautilus extensions that provide context menu items.
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

/* This interface is implemented by Nautilus extensions that want to
 * add context menu entries to files.  Extensions are called when
 * Nautilus constructs the context menu for a file.  They are passed a
 * list of NautilusFileInfo objects which holds the current selection */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include "nautilus-file-info.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_MENU_PROVIDER (nautilus_menu_provider_get_type ())

G_DECLARE_INTERFACE (NautilusMenuProvider, nautilus_menu_provider,
                     NAUTILUS, MENU_PROVIDER,
                     GObject)

struct _NautilusMenuProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_file_items)       (NautilusMenuProvider *provider,
                                    GList                *files);
    GList *(*get_background_items) (NautilusMenuProvider *provider,
                                    NautilusFileInfo     *current_folder);
};

GList  *nautilus_menu_provider_get_file_items           (NautilusMenuProvider *provider,
                                                         GList                *files);

GList *nautilus_menu_provider_get_background_items      (NautilusMenuProvider *provider,
                                                         NautilusFileInfo     *current_folder);

void   nautilus_menu_provider_emit_items_updated_signal (NautilusMenuProvider *provider);

G_END_DECLS
