/*
 *  nautilus-trash-monitor.c: Nautilus trash state watcher.
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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
 *  Author: Pavel Cisler <pavel@eazel.com>
 */

#include "nautilus-trash-monitor.h"

#include <eel/eel-debug.h>

#define UPDATE_RATE_SECONDS 1

struct _NautilusTrashMonitor
{
    GObject object;

    gboolean empty;
    GFileMonitor *file_monitor;
    gboolean pending;
    gint timeout_id;
};

enum
{
    TRASH_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static NautilusTrashMonitor *nautilus_trash_monitor = NULL;

G_DEFINE_TYPE (NautilusTrashMonitor, nautilus_trash_monitor, G_TYPE_OBJECT)

static void
nautilus_trash_monitor_finalize (GObject *object)
{
    NautilusTrashMonitor *trash_monitor;

    trash_monitor = NAUTILUS_TRASH_MONITOR (object);

    if (trash_monitor->timeout_id > 0)
    {
        g_source_remove (trash_monitor->timeout_id);
        trash_monitor->timeout_id = 0;
    }

    if (trash_monitor->file_monitor)
    {
        g_object_unref (trash_monitor->file_monitor);
    }

    G_OBJECT_CLASS (nautilus_trash_monitor_parent_class)->finalize (object);
}

static void
nautilus_trash_monitor_class_init (NautilusTrashMonitorClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_trash_monitor_finalize;

    signals[TRASH_STATE_CHANGED] = g_signal_new
                                       ("trash-state-changed",
                                       G_TYPE_FROM_CLASS (object_class),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__BOOLEAN,
                                       G_TYPE_NONE, 1,
                                       G_TYPE_BOOLEAN);
}

static void
update_empty_info (NautilusTrashMonitor *trash_monitor,
                   gboolean              is_empty)
{
    if (trash_monitor->empty == is_empty)
    {
        return;
    }

    trash_monitor->empty = is_empty;

    /* trash got empty or full, notify everyone who cares */
    g_signal_emit (trash_monitor,
                   signals[TRASH_STATE_CHANGED], 0,
                   trash_monitor->empty);
}

/* Use G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT since we only want to know whether the
 * trash is empty or not, not access its children. This is available for the
 * trash backend since it uses a cache. In this way we prevent flooding the
 * trash backend with enumeration requests when trashing > 1000 files
 */
static void
trash_query_info_cb (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    NautilusTrashMonitor *trash_monitor = user_data;
    GFileInfo *info;
    guint32 item_count;
    gboolean is_empty = TRUE;

    info = g_file_query_info_finish (G_FILE (source), res, NULL);

    if (info != NULL)
    {
        item_count = g_file_info_get_attribute_uint32 (info,
                                                       G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
        is_empty = item_count == 0;

        g_object_unref (info);
    }

    update_empty_info (trash_monitor, is_empty);

    g_object_unref (trash_monitor);
}

static void schedule_update_info (NautilusTrashMonitor *trash_monitor);

static gboolean
schedule_update_info_cb (gpointer data)
{
    NautilusTrashMonitor *trash_monitor = data;

    trash_monitor->timeout_id = 0;
    if (trash_monitor->pending)
    {
        trash_monitor->pending = FALSE;
        schedule_update_info (trash_monitor);
    }

    return G_SOURCE_REMOVE;
}

static void
schedule_update_info (NautilusTrashMonitor *trash_monitor)
{
    GFile *location;

    /* Rate limit the updates to not flood the gvfsd-trash when too many changes
     * happended in a short time.
     */
    if (trash_monitor->timeout_id > 0)
    {
        trash_monitor->pending = TRUE;
        return;
    }

    location = g_file_new_for_uri ("trash:///");
    g_file_query_info_async (location,
                             G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT,
                             G_FILE_QUERY_INFO_NONE,
                             G_PRIORITY_DEFAULT, NULL,
                             trash_query_info_cb, g_object_ref (trash_monitor));

    trash_monitor->timeout_id = g_timeout_add_seconds (UPDATE_RATE_SECONDS,
                                                                schedule_update_info_cb,
                                                                trash_monitor);
    g_object_unref (location);
}

static void
file_changed (GFileMonitor      *monitor,
              GFile             *child,
              GFile             *other_file,
              GFileMonitorEvent  event_type,
              gpointer           user_data)
{
    NautilusTrashMonitor *trash_monitor;

    trash_monitor = NAUTILUS_TRASH_MONITOR (user_data);

    schedule_update_info (trash_monitor);
}

static void
nautilus_trash_monitor_init (NautilusTrashMonitor *trash_monitor)
{
    GFile *location;

    trash_monitor->empty = TRUE;

    location = g_file_new_for_uri ("trash:///");

    trash_monitor->file_monitor = g_file_monitor_file (location, 0, NULL, NULL);
    trash_monitor->pending = FALSE;
    trash_monitor->timeout_id = 0;

    g_signal_connect (trash_monitor->file_monitor, "changed",
                      (GCallback) file_changed, trash_monitor);

    g_object_unref (location);

    schedule_update_info (trash_monitor);
}

static void
unref_trash_monitor (void)
{
    g_object_unref (nautilus_trash_monitor);
}

NautilusTrashMonitor *
nautilus_trash_monitor_get (void)
{
    if (nautilus_trash_monitor == NULL)
    {
        /* not running yet, start it up */

        nautilus_trash_monitor = NAUTILUS_TRASH_MONITOR
                                     (g_object_new (NAUTILUS_TYPE_TRASH_MONITOR, NULL));
        eel_debug_call_at_shutdown (unref_trash_monitor);
    }

    return nautilus_trash_monitor;
}

gboolean
nautilus_trash_monitor_is_empty (void)
{
    NautilusTrashMonitor *monitor;

    monitor = nautilus_trash_monitor_get ();
    return monitor->empty;
}

GIcon *
nautilus_trash_monitor_get_icon (void)
{
    gboolean empty;

    empty = nautilus_trash_monitor_is_empty ();

    if (empty)
    {
        return g_themed_icon_new ("user-trash-symbolic");
    }
    else
    {
        return g_themed_icon_new ("user-trash-full-symbolic");
    }
}
