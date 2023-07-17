/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-properties-model-provider.h"

/**
 * NautilusPropertiesModelProvider:
 *
 * Interface to provide additional properties.
 *
 * `NautilusPropertiesModelProvider` allows extensions to provide additional
 * information for the file properties.
 */

G_DEFINE_INTERFACE (NautilusPropertiesModelProvider, nautilus_properties_model_provider, G_TYPE_OBJECT)

static void
nautilus_properties_model_provider_default_init (NautilusPropertiesModelProviderInterface *klass)
{
}

/**
 * nautilus_properties_model_provider_get_models:
 * @files: (element-type NautilusFileInfo): a list of files
 *
 * This function is called by the application when it wants properties models
 * from the extension.
 *
 * This function is called in the main thread before the Properties are shown,
 * so it should return quickly. The models can be populated and updated
 * asynchronously.
 *
 * Returns: (nullable) (element-type NautilusPropertiesModel) (transfer full): a list of models.
 */
GList *
nautilus_properties_model_provider_get_models (NautilusPropertiesModelProvider *self,
                                               GList                           *files)
{
    NautilusPropertiesModelProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_PROPERTIES_MODEL_PROVIDER (self), NULL);

    iface = NAUTILUS_PROPERTIES_MODEL_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_models != NULL, NULL);

    return iface->get_models (self, files);
}
