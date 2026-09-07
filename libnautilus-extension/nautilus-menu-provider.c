/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
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
