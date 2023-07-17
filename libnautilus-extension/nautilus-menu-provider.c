/*
 *  nautilus-property-page-provider.c - Interface for Nautilus extensions
 *                                      that provide context menu items
 *                                      for files.
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

#include <config.h>
#include "nautilus-menu-provider.h"

/**
 * NautilusMenuProvider:
 *
 * Interface to provide additional menu items.
 *
 * `NautilusMenuProvider` allows extensions to provide additional menu items
 * in the file manager menus.
 */

G_DEFINE_INTERFACE (NautilusMenuProvider, nautilus_menu_provider, G_TYPE_OBJECT)

enum
{
    ITEMS_UPDATED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
nautilus_menu_provider_default_init (NautilusMenuProviderInterface *klass)
{
    /**
     * NautilusMenuProvider::items-updated:
     *
     * A signal to be emitted whenever the extension modifies the list of menu items.
     */
    signals[ITEMS_UPDATED] = g_signal_new ("items-updated",
                                           NAUTILUS_TYPE_MENU_PROVIDER,
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);
}

/**
 * nautilus_menu_provider_get_file_items:
 * @files: (element-type NautilusFileInfo): a list of selected files
 *
 * Called whenever the selected files in a view changes.
 *
 * Returns: (nullable) (element-type NautilusMenuItem) (transfer full): the provided list of items.
 */
GList *
nautilus_menu_provider_get_file_items (NautilusMenuProvider *provider,
                                       GList                *files)
{
    NautilusMenuProviderInterface *iface;

    iface = NAUTILUS_MENU_PROVIDER_GET_IFACE (provider);

    g_return_val_if_fail (NAUTILUS_IS_MENU_PROVIDER (provider), NULL);

    if (iface->get_file_items != NULL)
    {
        return iface->get_file_items (provider, files);
    }

    return NULL;
}

/**
 * nautilus_menu_provider_get_background_items:
 * @current_folder: the folder for which background items are requested
 *
 * Called at least once whenever the current view changes.
 *
 * Returns: (nullable) (element-type NautilusMenuItem) (transfer full): the provided list of items.
 */
GList *
nautilus_menu_provider_get_background_items (NautilusMenuProvider *provider,
                                             NautilusFileInfo     *current_folder)
{
    NautilusMenuProviderInterface *iface;

    iface = NAUTILUS_MENU_PROVIDER_GET_IFACE (provider);

    g_return_val_if_fail (NAUTILUS_IS_MENU_PROVIDER (provider), NULL);
    g_return_val_if_fail (NAUTILUS_IS_FILE_INFO (current_folder), NULL);

    if (iface->get_background_items != NULL)
    {
        return iface->get_background_items (provider, current_folder);
    }

    return NULL;
}

/**
 * nautilus_menu_provider_emit_items_updated_signal:
 *
 * Emits [signal@MenuProvider::items-updated].
 */
void
nautilus_menu_provider_emit_items_updated_signal (NautilusMenuProvider *provider)
{
    g_return_if_fail (NAUTILUS_IS_MENU_PROVIDER (provider));

    g_signal_emit (provider, signals[ITEMS_UPDATED], 0);
}
