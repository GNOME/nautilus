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

struct _NautilusPropertiesModelProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_models) (NautilusPropertiesModelProvider *provider,
                          GList                           *files);
};

GList *nautilus_properties_model_provider_get_models (NautilusPropertiesModelProvider *provider,
                                                      GList                           *files);

G_END_DECLS
