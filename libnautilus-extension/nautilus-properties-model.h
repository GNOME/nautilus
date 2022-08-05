/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTIES_MODEL (nautilus_properties_model_get_type ())

G_DECLARE_FINAL_TYPE (NautilusPropertiesModel,
                      nautilus_properties_model,
                      NAUTILUS, PROPERTIES_MODEL,
                      GObject)

/**
 * SECTION:nautilus-properties-model
 * @title: NautilusPropertiesModel
 * @short_description: Properties set descriptor model
 *
 * #NautilusPropertiesModel is an model that describes a set of file properties.
 * Extensions can provide #NautilusPropertiesModel objects by registering a
 * #NautilusPropertiesModelProvider and returning them from
 * nautilus_properties_model_provider_get_models(), which will be called by
 * the main application when creating file properties.
 */

/**
 * nautilus_properties_model_new:
 * @title: the user-visible name for the set of properties in this model
 * @model: a #GListModel containing #NautilusPropertyItem objects.
 *
 * Returns: (transfer full): a new #NautilusPropertiesModel
 */
NautilusPropertiesModel *nautilus_properties_model_new (const char *title,
                                                        GListModel *model);

/**
 * nautilus_properties_model_get_title:
 * @self: the properties model
 *
 * Returns: (transfer none): the title of this #NautilusPropertiesModel
 */
const char *nautilus_properties_model_get_title (NautilusPropertiesModel *self);

/**
 * nautilus_properties_model_set_title:
 * @self: the properties model
 * @title: the new title of this #NautilusPropertiesModel
 */
void nautilus_properties_model_set_title (NautilusPropertiesModel *self,
                                          const char              *title);

/**
 * nautilus_properties_model_get_model:
 * @self: the properties model
 *
 * Returns: (transfer none): a #GListModel containing #NautilusPropertiesItem.
 */
GListModel * nautilus_properties_model_get_model (NautilusPropertiesModel *self);


G_END_DECLS
