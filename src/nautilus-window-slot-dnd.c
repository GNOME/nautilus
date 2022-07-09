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

#include "nautilus-notebook.h"
#include "nautilus-application.h"
#include "nautilus-files-view-dnd.h"
#include "nautilus-window-slot-dnd.h"

typedef struct
{
    NautilusFile *target_file;
    NautilusWindowSlot *target_slot;
    GtkWidget *widget;

    gboolean is_notebook;
    guint switch_location_timer;
} NautilusDragSlotProxyInfo;

static void
switch_tab (NautilusDragSlotProxyInfo *drag_info)
{
    GtkWidget *notebook, *slot;
    gint idx, n_pages;

    if (drag_info->target_slot == NULL)
    {
        return;
    }

    notebook = gtk_widget_get_ancestor (GTK_WIDGET (drag_info->target_slot), GTK_TYPE_NOTEBOOK);
    n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

    for (idx = 0; idx < n_pages; idx++)
    {
        slot = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), idx);
        if (NAUTILUS_WINDOW_SLOT (slot) == drag_info->target_slot)
        {
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), idx);
            break;
        }
    }
}

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

    if (drag_info->is_notebook)
    {
        switch_tab (drag_info);
    }
    else
    {
        switch_location (drag_info);
    }

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
    NautilusDragSlotProxyInfo *drag_info;
    NautilusWindowSlot *target_slot;
    GtkRoot *window;
    GdkDragAction action;
    char *target_uri;
    GFile *location;
    const GValue *value;

    drag_info = user_data;

    action = 0;

    value = gtk_drop_target_get_value (target);
    if (value == NULL)
    {
        return 0;
    }

    window = gtk_widget_get_root (drag_info->widget);
    g_assert (NAUTILUS_IS_WINDOW (window));

    target_uri = NULL;
    if (drag_info->target_file != NULL)
    {
        target_uri = nautilus_file_get_uri (drag_info->target_file);
    }
    else
    {
        if (drag_info->target_slot != NULL)
        {
            target_slot = drag_info->target_slot;
        }
        else
        {
            target_slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (window));
        }

        if (target_slot != NULL)
        {
            location = nautilus_window_slot_get_location (target_slot);
            target_uri = g_file_get_uri (location);
        }
    }

    if (target_uri != NULL)
    {
        NautilusFile *file;
        NautilusDirectory *directory;
        gboolean can;
        file = nautilus_file_get_existing_by_uri (target_uri);
        directory = nautilus_directory_get_for_file (file);
        can = nautilus_file_can_write (file) && nautilus_directory_is_editable (directory);
        nautilus_directory_unref (directory);
        g_object_unref (file);
        if (!can)
        {
            action = 0;
            goto out;
        }

        if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
        {
            GSList *items = g_value_get_boxed (value);
            action = nautilus_dnd_get_preferred_action (file, items->data);
        }
    }

    g_free (target_uri);

out:
    slot_proxy_check_switch_location_timer (drag_info);

    return action;
}

static void
drag_info_free (gpointer user_data)
{
    NautilusDragSlotProxyInfo *drag_info = user_data;

    g_clear_object (&drag_info->target_file);
    g_clear_object (&drag_info->target_slot);

    g_slice_free (NautilusDragSlotProxyInfo, drag_info);
}

static void
drag_info_clear (NautilusDragSlotProxyInfo *drag_info)
{
    slot_proxy_remove_switch_location_timer (drag_info);
}

static void
slot_proxy_drag_leave (GtkDropTarget *target,
                       gpointer       user_data)
{
    NautilusDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    drag_info_clear (drag_info);
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
    char *target_uri;
    GList *uri_list = NULL;
    GFile *location;
    NautilusDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    window = gtk_widget_get_root (drag_info->widget);
    g_assert (NAUTILUS_IS_WINDOW (window));

    if (drag_info->target_slot != NULL)
    {
        target_slot = drag_info->target_slot;
    }
    else
    {
        target_slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (window));
    }

    target_uri = NULL;
    if (drag_info->target_file != NULL)
    {
        target_uri = nautilus_file_get_uri (drag_info->target_file);
    }
    else if (target_slot != NULL)
    {
        location = nautilus_window_slot_get_location (target_slot);
        target_uri = g_file_get_uri (location);
    }

    target_view = NULL;
    if (target_slot != NULL)
    {
        NautilusView *view;

        view = nautilus_window_slot_get_current_view (target_slot);

        if (view && NAUTILUS_IS_FILES_VIEW (view))
        {
            target_view = NAUTILUS_FILES_VIEW (view);
        }
    }

    if (target_slot != NULL && target_view != NULL)
    {
        if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
        {
            GSList *items = g_value_get_boxed (value);
            for (GSList *l = items; l != NULL; l = l->next)
            {
                uri_list = g_list_prepend (uri_list, g_file_get_uri (l->data));
            }

            nautilus_files_view_drop_proxy_received_uris (target_view,
                                                          uri_list,
                                                          target_uri,
                                                          gdk_drop_get_actions (gtk_drop_target_get_current_drop (target)));
            g_list_free_full (uri_list, g_free);
        }
    }
    g_free (target_uri);

    drag_info_clear (drag_info);
}

void
nautilus_drag_slot_proxy_init (GtkWidget          *widget,
                               NautilusFile       *target_file,
                               NautilusWindowSlot *target_slot)
{
    NautilusDragSlotProxyInfo *drag_info;
    GtkDropTarget *target;

    g_assert (GTK_IS_WIDGET (widget));

    drag_info = g_slice_new0 (NautilusDragSlotProxyInfo);

    g_object_set_data_full (G_OBJECT (widget), "drag-slot-proxy-data", drag_info,
                            drag_info_free);

    drag_info->is_notebook = (g_object_get_data (G_OBJECT (widget), "nautilus-notebook-tab") != NULL);

    if (target_file != NULL)
    {
        drag_info->target_file = nautilus_file_ref (target_file);
    }

    if (target_slot != NULL)
    {
        drag_info->target_slot = g_object_ref (target_slot);
    }

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
