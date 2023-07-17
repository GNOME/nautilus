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

NautilusPropertiesModel *nautilus_properties_model_new (const char *title,
                                                        GListModel *model);

const char *nautilus_properties_model_get_title (NautilusPropertiesModel *self);

void nautilus_properties_model_set_title (NautilusPropertiesModel *self,
                                          const char              *title);

GListModel * nautilus_properties_model_get_model (NautilusPropertiesModel *self);


G_END_DECLS
