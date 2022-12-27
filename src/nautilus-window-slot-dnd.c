/*
 * nautilus-window-slot-dnd.c - Handle DnD for widgets acting as
 * NautilusWindowSlot proxies
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 *      Ettore Perazzoli <ettore@gnu.org>
 */

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-files-view-dnd.h"
#include "nautilus-window-slot-dnd.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

typedef struct
{
    NautilusFile *target_file;
    GtkWidget *widget;

    graphene_point_t hover_start_point;
    guint switch_location_timer;
} NautilusDragSlotProxyInfo;

static void
switch_location (NautilusDragSlotProxyInfo *drag_info)
{
    GFile *location;
    GtkRoot *window;

    if (drag_info->target_file == NULL)
    {
        return;
    }

    window = gtk_widget_get_root (drag_info->widget);
    g_assert (NAUTILUS_IS_WINDOW (window));

    location = nautilus_file_get_location (drag_info->target_file);
    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             location, NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE,
                                             NULL, NAUTILUS_WINDOW (window), NULL);
    g_object_unref (location);
}

static gboolean
slot_proxy_switch_location_timer (gpointer user_data)
{
    NautilusDragSlotProxyInfo *drag_info = user_data;

    drag_info->switch_location_timer = 0;

    switch_location (drag_info);

    return FALSE;
}

static void
slot_proxy_check_switch_location_timer (NautilusDragSlotProxyInfo *drag_info)
{
    if (drag_info->switch_location_timer)
    {
        return;
    }

    drag_info->switch_location_timer = g_timeout_add (HOVER_TIMEOUT,
                                                      slot_proxy_switch_location_timer,
                                                      drag_info);
}

static void
slot_proxy_remove_switch_location_timer (NautilusDragSlotProxyInfo *drag_info)
{
    if (drag_info->switch_location_timer != 0)
    {
        g_source_remove (drag_info->switch_location_timer);
        drag_info->switch_location_timer = 0;
    }
}

static GdkDragAction
slot_proxy_drag_motion (GtkDropTarget *target,
                        gdouble        x,
                        gdouble        y,
                        gpointer       user_data)
{
    NautilusDragSlotProxyInfo *drag_info = user_data;
    GdkDragAction action;
    const GValue *value;
    graphene_point_t start;

    value = gtk_drop_target_get_value (target);
    if (value == NULL)
    {
        return 0;
    }

    if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *items = g_value_get_boxed (value);

        if (items == NULL)
        {
            action = 0;
        }
        else
        {
            action = nautilus_dnd_get_preferred_action (drag_info->target_file, items->data);
        }
    }

    start = drag_info->hover_start_point;
    if (gtk_drag_check_threshold (drag_info->widget, start.x, start.y, x, y))
    {
        slot_proxy_remove_switch_location_timer (drag_info);
        slot_proxy_check_switch_location_timer (drag_info);
        drag_info->hover_start_point.x = x;
        drag_info->hover_start_point.y = y;
    }

    return action;
}

static void
drag_info_free (gpointer user_data)
{
    NautilusDragSlotProxyInfo *drag_info = user_data;

    slot_proxy_remove_switch_location_timer (drag_info);

    g_clear_object (&drag_info->target_file);

    g_slice_free (NautilusDragSlotProxyInfo, drag_info);
}


static void
slot_proxy_drag_leave (GtkDropTarget *target,
                       gpointer       user_data)
{
    NautilusDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    slot_proxy_remove_switch_location_timer (drag_info);
}

static void
slot_proxy_handle_drop (GtkDropTarget *target,
                        const GValue  *value,
                        gdouble        x,
                        gdouble        y,
                        gpointer       user_data)
{
    GtkRoot *window;
    NautilusWindowSlot *target_slot;
    NautilusFilesView *target_view;
    g_autoptr (GFile) target_location = NULL;
    NautilusDragSlotProxyInfo *drag_info = user_data;
    GdkDragAction action;

    window = gtk_widget_get_root (drag_info->widget);
    g_assert (NAUTILUS_IS_WINDOW (window));

    target_slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (window));
    target_location = nautilus_file_get_location (drag_info->target_file);
    target_view = NAUTILUS_FILES_VIEW (nautilus_window_slot_get_current_view (target_slot));
    action = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));

    #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        /* Temporary workaround until the below GTK MR (or equivalent fix)
         * is merged.  Without this fix, the preferred action isn't set correctly.
         * https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4982 */
        GdkDrag *drag = gdk_drop_get_drag (gtk_drop_target_get_current_drop (target));
        action = gdk_drag_get_selected_action (drag);
    }
    #endif

    nautilus_dnd_perform_drop (target_view, value, action, target_location);

    drag_info_free (drag_info);
}

void
nautilus_drag_slot_proxy_init (GtkWidget    *widget,
                               NautilusFile *target_file)
{
    NautilusDragSlotProxyInfo *drag_info;
    GtkDropTarget *target;

    g_assert (GTK_IS_WIDGET (widget));

    drag_info = g_slice_new0 (NautilusDragSlotProxyInfo);

    drag_info->target_file = nautilus_file_ref (target_file);

    drag_info->widget = widget;
    /* TODO: Implement GDK_ACTION_ASK */
    target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);

    gtk_drop_target_set_preload (target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (target, (GType[1]) { GDK_TYPE_FILE_LIST }, 1);
    g_signal_connect (target, "enter", G_CALLBACK (slot_proxy_drag_motion), drag_info);
    g_signal_connect (target, "motion", G_CALLBACK (slot_proxy_drag_motion), drag_info);
    g_signal_connect (target, "drop", G_CALLBACK (slot_proxy_handle_drop), drag_info);
    g_signal_connect (target, "leave", G_CALLBACK (slot_proxy_drag_leave), drag_info);
    gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (target));
}
