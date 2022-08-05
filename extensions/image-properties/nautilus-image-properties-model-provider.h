/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

#define NAUTILUS_TYPE_IMAGE_PROPERTIES_MODEL_PROVIDER (nautilus_image_properties_model_provider_get_type ())

G_DECLARE_FINAL_TYPE (NautilusImagesPropertiesModelProvider,
                      nautilus_image_properties_model_provider,
                      NAUTILUS, IMAGE_PROPERTIES_MODEL_PROVIDER,
                      GObject)

void nautilus_image_properties_model_provider_load (GTypeModule *module);
