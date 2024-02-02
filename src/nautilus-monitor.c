/*
 *  nautilus-monitor.c: file and directory change monitoring for nautilus
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2016 Red Hat
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Seth Nickell <seth@eazel.com>
 *           Darin Adler <darin@bentspoon.com>
 *           Alex Graveley <alex@ximian.com>
 *           Carlos Soriano <csoriano@gnome.org>
 */

#include <config.h>
#include "nautilus-monitor.h"
#include "nautilus-file-changes-queue.h"
#include "nautilus-file-utilities.h"

#include <gio/gio.h>

struct NautilusMonitor
{
    GFileMonitor *monitor;
    GVolumeMonitor *volume_monitor;
    GFile *location;
};

G_LOCK_DEFINE_STATIC (monitor_lock);

static GHashTable *moved_hints = NULL;
static gboolean call_consume_changes_idle_id = 0;

static gboolean
call_consume_changes_idle_cb (gpointer not_used)
{
    nautilus_file_changes_consume_changes ();
    call_consume_changes_idle_id = 0;
    return FALSE;
}

static void
schedule_call_consume_changes (void)
{
    if (call_consume_changes_idle_id == 0)
    {
        call_consume_changes_idle_id =
            g_idle_add (call_consume_changes_idle_cb, NULL);
    }
}

static void
mount_removed (GVolumeMonitor *volume_monitor,
               GMount         *mount,
               gpointer        user_data)
{
    NautilusMonitor *monitor = user_data;
    GFile *mount_location;

    mount_location = g_mount_get_root (mount);
    if (g_file_equal (monitor->location, mount_location) ||
        g_file_has_prefix (monitor->location, mount_location))
    {
        nautilus_file_changes_queue_file_unmounted (monitor->location);
        schedule_call_consume_changes ();
    }

    g_object_unref (mount_location);
}

static void
ensure_moved_files_hint_hash (void)
{
    static gsize init_value = 0;

    if (g_once_init_enter (&init_value))
    {
        moved_hints = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
                                             g_object_unref, g_object_unref);
        g_once_init_leave (&init_value, 1);
    }
}

static GFile *
get_moved_hint_src_from_dest (GFile *location)
{
    GHashTableIter iter;
    gpointer key, value;

    G_LOCK (monitor_lock);
    g_hash_table_iter_init (&iter, moved_hints);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        if (g_file_equal (location, value))
        {
            G_UNLOCK (monitor_lock);
            return key;
        }
    }
    G_UNLOCK (monitor_lock);

    return NULL;
}

static void
dir_changed (GFileMonitor      *monitor,
             GFile             *child,
             GFile             *other_file,
             GFileMonitorEvent  event_type,
             gpointer           user_data)
{
    switch (event_type)
    {
        default:
        case G_FILE_MONITOR_EVENT_CHANGED:
        {
            /* ignore */
        }
        break;

        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        {
            nautilus_file_changes_queue_file_changed (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_UNMOUNTED:
        {
            nautilus_file_changes_queue_file_unmounted (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_DELETED:
        {
            nautilus_file_changes_queue_file_removed (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_CREATED:
        {
            nautilus_file_changes_queue_file_added (child);
        }
        break;

        case G_FILE_MONITOR_EVENT_MOVED_IN:
        {
            if (other_file == NULL)
            {
                other_file = get_moved_hint_src_from_dest (child);
            }
            if (other_file != NULL)
            {
                nautilus_file_changes_queue_file_moved (other_file, child);
            }
            else
            {
                nautilus_file_changes_queue_file_added (child);
            }
        }
        break;

        case G_FILE_MONITOR_EVENT_MOVED_OUT:
        {
            if (other_file == NULL)
            {
                G_LOCK (monitor_lock);
                other_file = g_hash_table_lookup (moved_hints, child);
                G_UNLOCK (monitor_lock);
            }
            if (other_file != NULL)
            {
                nautilus_file_changes_queue_file_moved (child, other_file);
            }
            else
            {
                nautilus_file_changes_queue_file_removed (child);
            }
        }
        break;

        case G_FILE_MONITOR_EVENT_RENAMED:
        {
            nautilus_file_changes_queue_file_moved (child, other_file);
        }
        break;
    }

    schedule_call_consume_changes ();
}

NautilusMonitor *
nautilus_monitor_directory (GFile *location)
{
    GFileMonitor *dir_monitor;
    NautilusMonitor *ret;

    ret = g_slice_new0 (NautilusMonitor);
    dir_monitor = g_file_monitor_directory (location,
                                            G_FILE_MONITOR_WATCH_MOUNTS | G_FILE_MONITOR_WATCH_MOVES,
                                            NULL, NULL);

    if (dir_monitor != NULL)
    {
        ret->monitor = dir_monitor;
    }

    ensure_moved_files_hint_hash ();

    /* Currently, some GVfs backends which support monitoring never emit
     * G_FILE_MONITOR_EVENT_UNMOUNTED, nor _DELETED events when the location
     * is unmounted. Use GVolumeMonitor in addition to GFileMonitor.
     */
    if (!g_file_is_native (location))
    {
        ret->location = g_object_ref (location);
        ret->volume_monitor = g_volume_monitor_get ();
    }

    if (ret->monitor != NULL)
    {
        g_signal_connect (ret->monitor, "changed",
                          G_CALLBACK (dir_changed), ret);
    }

    if (ret->volume_monitor != NULL)
    {
        g_signal_connect (ret->volume_monitor, "mount-removed",
                          G_CALLBACK (mount_removed), ret);
    }

    /* We return a monitor even on failure, so we can avoid later trying again */
    return ret;
}

void
nautilus_monitor_add_moved_hint (GFile *src,
                                 GFile *dest)
{
    ensure_moved_files_hint_hash ();

    G_LOCK (monitor_lock);
    g_hash_table_insert (moved_hints, g_object_ref (src), g_object_ref (dest));
    G_UNLOCK (monitor_lock);
}

void
nautilus_monitor_remove_moved_hint (GFile *src,
                                    GFile *dest)
{
    ensure_moved_files_hint_hash ();

    G_LOCK (monitor_lock);
    GFile *value = g_hash_table_lookup (moved_hints, src);

    if (value != NULL && g_file_equal (value, dest))
    {
        g_hash_table_remove (moved_hints, src);
    }
    G_UNLOCK (monitor_lock);
}

void
nautilus_monitor_cancel (NautilusMonitor *monitor)
{
    if (monitor->monitor != NULL)
    {
        g_signal_handlers_disconnect_by_func (monitor->monitor, dir_changed, monitor);
        g_file_monitor_cancel (monitor->monitor);
        g_object_unref (monitor->monitor);
    }

    if (monitor->volume_monitor != NULL)
    {
        g_signal_handlers_disconnect_by_func (monitor->volume_monitor, mount_removed, monitor);
        g_object_unref (monitor->volume_monitor);
    }

    g_clear_object (&monitor->location);
    g_slice_free (NautilusMonitor, monitor);
}
