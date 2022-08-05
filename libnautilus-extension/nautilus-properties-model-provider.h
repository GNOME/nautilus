/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTIES_MODEL_PROVIDER (nautilus_properties_model_provider_get_type ())

G_DECLARE_INTERFACE (NautilusPropertiesModelProvider,
                     nautilus_properties_model_provider,
                     NAUTILUS, PROPERTIES_MODEL_PROVIDER,
                     GObject)

/**
 * SECTION:nautilus-properties-model-provider
 * @title: NautilusPropertiesModelProvider
 * @short_description: Interface to provide additional properties
 *
 * #NautilusPropertiesModelProvider allows extension to provide additional
 * information for the file properties.
 */

/**
 * NautilusPropertiesModelProviderInterface:
 * @g_iface: The parent interface.
 * @get_models: Returns a #GList of #NautilusPropertiesModel.
 *   See nautilus_properties_model_provider_get_models() for details.
 *
 * Interface for extensions to provide additional properties.
 */
struct _NautilusPropertiesModelProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_models) (NautilusPropertiesModelProvider *provider,
                          GList                           *files);
};

/**
 * nautilus_properties_model_provider_get_models:
 * @provider: a #NautilusPropertiesModelProvider
 * @files: (element-type NautilusFileInfo): a #GList of #NautilusFileInfo
 *
 * This function is called by the application when it wants properties models
 * from the extension.
 *
 * This function is called in the main thread before the Properties are shown,
 * so it should return quickly. The models can be populated and updated
 * asynchronously.
 *
 * Returns: (nullable) (element-type NautilusPropertyModel) (transfer full): A #GList of allocated #NautilusPropertiesModel models.
 */
GList *nautilus_properties_model_provider_get_models (NautilusPropertiesModelProvider *provider,
                                                      GList                           *files);

G_END_DECLS
