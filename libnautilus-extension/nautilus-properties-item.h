/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTIES_ITEM (nautilus_properties_item_get_type ())

G_DECLARE_FINAL_TYPE (NautilusPropertiesItem,
                      nautilus_properties_item,
                      NAUTILUS, PROPERTIES_ITEM,
                      GObject)

/**
 * SECTION:nautilus-properties-item
 * @name: NautilusPropertiesItem
 * @value: Properties item descriptor object
 *
 * #NautilusPropertiesItem is an object that describes a name & value pair in
 * file properties. Extensions can provide #NautilusPropertiesItem objects in
 * models provided by #NautilusPropertiesModel.
 */

/**
 * nautilus_properties_item_new:
 * @name: the user-visible name for the properties item.
 * @model: the user-visible value for the properties item.
 *
 * Returns: (transfer full): a new #NautilusPropertiesItem
 */
NautilusPropertiesItem *nautilus_properties_item_new (const char *name,
                                                      const char *value);

/**
 * nautilus_properties_item_get_name:
 * @item: the properties item
 *
 * Returns: (transfer none): the name of this #NautilusPropertiesItem
 */
const char *nautilus_properties_item_get_name (NautilusPropertiesItem *self);

/**
 * nautilus_properties_item_get_value:
 * @item: the properties item
 *
 * Returns: (transfer none): the value of this #NautilusPropertiesItem
 */
const char * nautilus_properties_item_get_value (NautilusPropertiesItem *self);


G_END_DECLS
