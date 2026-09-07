/*
 * SPDX-FileCopyrightText: 2020 Alberts Muktupāvels
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>

typedef struct _NautilusFileOperationsDBusData NautilusFileOperationsDBusData;

NautilusFileOperationsDBusData *nautilus_file_operations_dbus_data_new               (GVariant                       *platform_data);

NautilusFileOperationsDBusData *nautilus_file_operations_dbus_data_ref               (NautilusFileOperationsDBusData *self);

void                            nautilus_file_operations_dbus_data_unref             (NautilusFileOperationsDBusData *self);

const char                     *nautilus_file_operations_dbus_data_get_parent_handle (NautilusFileOperationsDBusData *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusFileOperationsDBusData, nautilus_file_operations_dbus_data_unref)
