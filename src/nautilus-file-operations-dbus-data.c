/*
 * SPDX-FileCopyrightText: 2020 Alberts Muktupāvels
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include "nautilus-file-operations-dbus-data.h"

struct _NautilusFileOperationsDBusData
{
    gatomicrefcount ref_count;

    char *parent_handle;
};

NautilusFileOperationsDBusData *
nautilus_file_operations_dbus_data_new (GVariant *platform_data)
{
    NautilusFileOperationsDBusData *self;
    GVariantDict dict;

    self = g_new0 (NautilusFileOperationsDBusData, 1);
    g_atomic_ref_count_init (&self->ref_count);

    g_variant_dict_init (&dict, platform_data);

    g_variant_dict_lookup (&dict, "parent-handle", "s", &self->parent_handle);

    return self;
}

NautilusFileOperationsDBusData *
nautilus_file_operations_dbus_data_ref (NautilusFileOperationsDBusData *self)
{
    g_atomic_ref_count_inc (&self->ref_count);

    return self;
}

void
nautilus_file_operations_dbus_data_unref (NautilusFileOperationsDBusData *self)
{
    if (g_atomic_ref_count_dec (&self->ref_count))
    {
        g_free (self->parent_handle);
        g_free (self);
    }
}

const char *
nautilus_file_operations_dbus_data_get_parent_handle (NautilusFileOperationsDBusData *self)
{
    return self->parent_handle;
}
