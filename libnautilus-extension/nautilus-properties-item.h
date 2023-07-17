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

NautilusPropertiesItem *nautilus_properties_item_new (const char *name,
                                                      const char *value);

const char *nautilus_properties_item_get_name (NautilusPropertiesItem *self);

const char * nautilus_properties_item_get_value (NautilusPropertiesItem *self);


G_END_DECLS
