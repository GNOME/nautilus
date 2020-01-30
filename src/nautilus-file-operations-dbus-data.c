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

#include "config.h"
#include "nautilus-file-operations-dbus-data.h"

#include <gtk/gtk.h>

struct _NautilusFileOperationsDBusData
{
    gatomicrefcount  ref_count;

    char            *parent_handle;

    guint32          timestamp;

    int              window_position;
};

NautilusFileOperationsDBusData *
nautilus_file_operations_dbus_data_new (GVariant *platform_data)
{
    NautilusFileOperationsDBusData *self;
    GVariantDict dict;
    const char *window_position;

    self = g_new0 (NautilusFileOperationsDBusData, 1);
    g_atomic_ref_count_init (&self->ref_count);

    g_variant_dict_init (&dict, platform_data);

    g_variant_dict_lookup (&dict, "parent-handle", "s", &self->parent_handle);
    g_variant_dict_lookup (&dict, "timestamp", "u", &self->timestamp);

    self->window_position = -1;
    if (g_variant_dict_lookup (&dict, "window-position", "&s", &window_position))
    {
        if (g_strcmp0 (window_position, "none") == 0)
        {
            self->window_position = GTK_WIN_POS_NONE;
        }
        else if (g_strcmp0 (window_position, "center") == 0)
        {
            self->window_position = GTK_WIN_POS_CENTER;
        }
        else if (g_strcmp0 (window_position, "mouse") == 0)
        {
            self->window_position = GTK_WIN_POS_MOUSE;
        }
        else if (g_strcmp0 (window_position, "center-always") == 0)
        {
            self->window_position = GTK_WIN_POS_CENTER_ALWAYS;
        }
        else if (g_strcmp0 (window_position, "center-on-parent") == 0)
        {
            self->window_position = GTK_WIN_POS_CENTER_ON_PARENT;
        }
    }

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

guint32
nautilus_file_operations_dbus_data_get_timestamp (NautilusFileOperationsDBusData *self)
{
    return self->timestamp;
}

int
nautilus_file_operations_dbus_data_get_window_position (NautilusFileOperationsDBusData *self)
{
    return self->window_position;
}
