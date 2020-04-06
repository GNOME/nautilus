/*
 * Copyright (C) 2020 Alberts Muktupāvels
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>

typedef struct _NautilusFileOperationsDBusData NautilusFileOperationsDBusData;

NautilusFileOperationsDBusData *nautilus_file_operations_dbus_data_new               (GVariant                       *platform_data);

NautilusFileOperationsDBusData *nautilus_file_operations_dbus_data_ref               (NautilusFileOperationsDBusData *self);

void                            nautilus_file_operations_dbus_data_unref             (NautilusFileOperationsDBusData *self);

const char                     *nautilus_file_operations_dbus_data_get_parent_handle (NautilusFileOperationsDBusData *self);

guint32                         nautilus_file_operations_dbus_data_get_timestamp     (NautilusFileOperationsDBusData *self);

void                            nautilus_file_operations_dbus_data_set_ask_confirmation (NautilusFileOperationsDBusData *self,
                                                                                         gboolean                        ask_confirmation);

gboolean                        nautilus_file_operations_dbus_data_get_ask_confirmation (NautilusFileOperationsDBusData *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(NautilusFileOperationsDBusData, nautilus_file_operations_dbus_data_unref)
