/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-properties-model-provider.h"

G_DEFINE_INTERFACE (NautilusPropertiesModelProvider, nautilus_properties_model_provider, G_TYPE_OBJECT)

static void
nautilus_properties_model_provider_default_init (NautilusPropertiesModelProviderInterface *klass)
{
}

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
