/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of libnautilus-extension.
 *
 * libnautilus-extension is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libnautilus-extension is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libnautilus-extension.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef NAUTILUS_EXTENSION_H
#define NAUTILUS_EXTENSION_H

#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-extension/nautilus-column.h>
#include <libnautilus-extension/nautilus-extension-enum-types.h>
#include <libnautilus-extension/nautilus-file-info.h>
#include <libnautilus-extension/nautilus-info-provider.h>
#include <libnautilus-extension/nautilus-menu.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-properties-model.h>
#include <libnautilus-extension/nautilus-properties-model-provider.h>
#include <libnautilus-extension/nautilus-properties-item.h>

/**
 * SECTION:nautilus-extension
 * @title: Extension entry points
 */

/**
 * nautilus_module_initialize: (skip)
 * @module: a #GTypeModule used in type registration
 *
 * Called when the extension is begin loaded to register the types it exports
 * and to perform other initializations.
 */
void nautilus_module_initialize (GTypeModule  *module);
/**
 * nautilus_module_shutdown: (skip)
 *
 * Called when the extension is being unloaded.
 */
void nautilus_module_shutdown   (void);
/**
 * nautilus_module_list_types: (skip)
 * @types: (out) (transfer none) (array length=num_types): array of GType *
 * @num_types: the number of types in the array
 *
 * Called after the extension has been initialized and has registered all the
 * types it exports, to load them into Nautilus.
 */
void nautilus_module_list_types (const GType **types,
                                 int          *num_types);

#endif
