/*
 *  nautilus-property-page-model-provider.c - Interface for Nautilus extensions
 * *                                      that provide property pages for
 * *                                      files.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *  Copyright (C) 2018 Red Hat, Inc.
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
 *           Carlos Soriano <csoriano@redhat.com>
 *
 */
#include "config.h"

#include "nautilus-property-page-model-provider.h"
#include "nautilus-property-page-model.h"

G_DEFINE_INTERFACE (NautilusPropertyPageModelProvider, nautilus_property_page_model_provider,
                    G_TYPE_OBJECT)

/**
 * SECTION:nautilus-property-page-model-provider-interface
 * @title: NautilusPropertyPageModelProvider
 * @short_description: Interface to provide a property page
 *
 * #NautilusPropertyPageModelProvider allows extension to provide an additional
 * page for the file properties dialog.
 */

static void
nautilus_property_page_model_provider_default_init (NautilusPropertyPageModelProviderInterface *klass)
{
}

/**
 * nautilus_property_page_model_provider_get_model:
 * @provider: a #NautilusPropertyPageModelProvider
 * @files: (element-type NautilusFileInfo): a #GList of #NautilusFileInfo
 *
 * This function is called by Nautilus when it wants property page
 * items from the extension.
 *
 * This function is called in the main thread before a property page
 * is shown, so it should return quickly.
 *
 * Returns: (element-type NautilusPropertyItem) (transfer full): A #GList of allocated #NautilusPropertyItem.
 */
NautilusPropertyPageModel *
nautilus_property_page_model_provider_get_model (NautilusPropertyPageModelProvider *self,
                                                 GList                             *files)
{
    NautilusPropertyPageModelProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_PROPERTY_PAGE_MODEL_PROVIDER (self), NULL);

    iface = NAUTILUS_PROPERTY_PAGE_MODEL_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_model != NULL, NULL);

    return iface->get_model (self, files);
}
