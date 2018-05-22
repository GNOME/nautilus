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
    /**
     * NautilusView::page-model:
     *
     * The page model associated with the provider.
     */
    g_object_interface_install_property (klass,
                                         g_param_spec_object ("page-model",
                                                              "Page model",
                                                              "The page model associated with the provider",
                                                              NAUTILUS_TYPE_PROPERTY_PAGE_MODEL,
                                                              G_PARAM_READWRITE));
    g_object_interface_install_property (klass,
                                         g_param_spec_pointer ("files",
                                                               "Files",
                                                               "Files to get info from",
                                                              G_PARAM_READWRITE));
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
nautilus_property_page_model_provider_get_model (NautilusPropertyPageModelProvider *self)
{
    NautilusPropertyPageModelProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_PROPERTY_PAGE_MODEL_PROVIDER (self), NULL);

    iface = NAUTILUS_PROPERTY_PAGE_MODEL_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_model != NULL, NULL);

    return iface->get_model (self);
}

void
nautilus_property_page_model_provider_set_files (NautilusPropertyPageModelProvider *self,
                                                 GList                             *files)
{
    NautilusPropertyPageModelProviderInterface *iface;

    g_return_if_fail (NAUTILUS_IS_PROPERTY_PAGE_MODEL_PROVIDER (self));

    iface = NAUTILUS_PROPERTY_PAGE_MODEL_PROVIDER_GET_IFACE (self);

    g_return_if_fail (iface->set_files != NULL);

    iface->set_files (self, files);
}

gboolean
nautilus_property_page_model_provider_supports_files (NautilusPropertyPageModelProvider *self)
{
    NautilusPropertyPageModelProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_PROPERTY_PAGE_MODEL_PROVIDER (self), FALSE);

    iface = NAUTILUS_PROPERTY_PAGE_MODEL_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->supports_files != NULL, FALSE);

    return iface->supports_files (self);
}
