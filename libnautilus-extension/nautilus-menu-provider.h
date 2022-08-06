/*
 *  nautilus-menu-provider.h - Interface for Nautilus extensions that 
 *                             provide context menu items.
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

/**
 * SECTION:nautilus-menu-provider
 * @title: NautilusMenuProvider
 * @short_description: Interface to provide additional menu items
 *
 * #NautilusMenuProvider allows extension to provide additional menu items
 * in the file manager menus.
 */

/**
 * NautilusMenuProviderInterface:
 * @g_iface: The parent interface.
 * @get_file_items: Returns a #GList of #NautilusMenuItem.
 *                  See nautilus_menu_provider_get_file_items() for details.
 * @get_background_items: Returns a #GList of #NautilusMenuItem.
 *                        See nautilus_menu_provider_get_background_items() for details.
 *
 * Interface for extensions to provide additional menu items.
 */
struct _NautilusMenuProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_file_items)       (NautilusMenuProvider *provider,
                                    GList                *files);
    GList *(*get_background_items) (NautilusMenuProvider *provider,
                                    NautilusFileInfo     *current_folder);
};

/**
 * nautilus_menu_provider_get_file_items:
 * @provider: a #NautilusMenuProvider
 * @files: (element-type NautilusFileInfo): a list of #NautilusFileInfo
 *
 * Returns: (nullable) (element-type NautilusMenuItem) (transfer full): the provided list of #NautilusMenuItem.
 */
GList  *nautilus_menu_provider_get_file_items           (NautilusMenuProvider *provider,
                                                         GList                *files);
/**
 * nautilus_menu_provider_get_background_items:
 * @provider: a #NautilusMenuProvider
 * @current_folder: the folder for which background items are requested
 *
 * Returns: (nullable) (element-type NautilusMenuItem) (transfer full): the provided list of #NautilusMenuItem.
 */
GList *nautilus_menu_provider_get_background_items      (NautilusMenuProvider *provider,
                                                         NautilusFileInfo     *current_folder);

/**
 * nautilus_menu_provider_emit_items_updated_signal:
 * @provider: a #NautilusMenuProvider
 *
 * Emits #NautilusMenuProvider::items-updated.
 */
void   nautilus_menu_provider_emit_items_updated_signal (NautilusMenuProvider *provider);

G_END_DECLS
