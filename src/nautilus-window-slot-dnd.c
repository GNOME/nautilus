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
    gboolean drop_occurred;

    union
    {
        GList *selection_list;
        GList *uri_list;
    } data;

    NautilusFile *target_file;
    NautilusWindowSlot *target_slot;
    GtkWidget *widget;

    gboolean is_notebook;
    guint switch_location_timer;

    GdkDragAction pending_actions;
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

    notebook = gtk_widget_get_ancestor (GTK_WIDGET (drag_info->target_slot), NAUTILUS_TYPE_NOTEBOOK);
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
    GtkWidget *window;

    if (drag_info->target_file == NULL)
    {
        return;
    }

    window = gtk_widget_get_toplevel (drag_info->widget);
    g_assert (NAUTILUS_IS_WINDOW (window));

    location = nautilus_file_get_location (drag_info->target_file);
    nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                             location, NAUTILUS_WINDOW_OPEN_FLAG_DONT_MAKE_ACTIVE,
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
slot_proxy_check_switch_location_timer (NautilusDragSlotProxyInfo *drag_info,
                                        GtkWidget                 *widget)
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

static gboolean
slot_proxy_drag_motion (GtkWidget *widget,
                        GdkDrop   *drop,
                        int        x,
                        int        y,
                        gpointer   user_data)
{
    NautilusDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    if (drag_info->pending_actions == 0)
    {
        GdkAtom target;

        target = gtk_drag_dest_find_target (widget, drop, NULL);
        if (target == NULL)
        {
            gdk_drop_status (drop, 0);
        }

        gtk_drag_get_data (widget, drop, target);
    }
    else
    {
        gdk_drop_status (drop, drag_info->pending_actions);
    }

    return TRUE;
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
drag_info_clear (NautilusDragSlotProxyInfo *drag_info,
                 GdkDrop                   *drop)
{
    GdkContentFormats *formats;

    formats = gdk_drop_get_formats (drop);

    slot_proxy_remove_switch_location_timer (drag_info);

    if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
    {
        g_clear_pointer (&drag_info->data.selection_list, nautilus_drag_destroy_selection_list);
    }
    else if (nautilus_content_formats_include_uri (formats))
    {
        g_clear_pointer (&drag_info->data.uri_list, g_list_free);
    }

    drag_info->drop_occurred = FALSE;
    drag_info->pending_actions = 0;
}

static void
slot_proxy_drag_leave (GtkWidget *widget,
                       GdkDrop   *drop,
                       gpointer   user_data)
{
    NautilusDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    gtk_drag_unhighlight (widget);

    if (!drag_info->drop_occurred)
    {
        drag_info_clear (drag_info, drop);
    }
}

static gboolean
slot_proxy_drag_drop (GtkWidget    *widget,
                      GdkDrop      *drop,
                      int           x,
                      int           y,
                      gpointer      user_data)
{
    GdkAtom target;
    NautilusDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    drag_info->drop_occurred = TRUE;

    target = gtk_drag_dest_find_target (widget, drop, NULL);
    gtk_drag_get_data (widget, drop, target);

    return TRUE;
}


static void
slot_proxy_handle_drop (GtkWidget                 *widget,
                        GdkDrop                   *drop,
                        NautilusDragSlotProxyInfo *drag_info)
{
    GdkContentFormats *formats;
    GtkWidget *window;
    NautilusWindowSlot *target_slot;
    NautilusFilesView *target_view;
    g_autofree char *target_uri = NULL;

    formats = gdk_drop_get_formats (drop);
    window = gtk_widget_get_toplevel (widget);
    g_assert (NAUTILUS_IS_WINDOW (window));

    if (drag_info->target_slot != NULL)
    {
        target_slot = drag_info->target_slot;
    }
    else
    {
        target_slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (window));
    }

    if (drag_info->target_file != NULL)
    {
        target_uri = nautilus_file_get_uri (drag_info->target_file);
    }
    else if (target_slot != NULL)
    {
        GFile *location;

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
        if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
        {
            GList *uri_list;

            uri_list = nautilus_drag_uri_list_from_selection_list (drag_info->data.selection_list);
            g_assert (uri_list != NULL);

            nautilus_files_view_drop_proxy_received_uris (target_view,
                                                          uri_list,
                                                          target_uri,
                                                          gdk_drop_get_actions (drop));
            g_list_free_full (uri_list, g_free);
        }
        else if (nautilus_content_formats_include_uri (formats))
        {
            nautilus_files_view_drop_proxy_received_uris (target_view,
                                                          drag_info->data.uri_list,
                                                          target_uri,
                                                          gdk_drop_get_actions (drop));
        }

        gdk_drop_finish (drop, gdk_drop_get_actions (drop));
    }
    else
    {
        gdk_drop_finish (drop, 0);
    }

    drag_info_clear (drag_info, drop);
}

static char *
get_target_uri (GtkWidget                 *widget,
                NautilusDragSlotProxyInfo *drag_info)
{
    if (drag_info->target_file != NULL)
    {
        return nautilus_file_get_uri (drag_info->target_file);
    }
    else
    {
        NautilusWindowSlot *target_slot;

        if (drag_info->target_slot != NULL)
        {
            target_slot = drag_info->target_slot;
        }
        else
        {
            GtkWidget *window;

            window = gtk_widget_get_toplevel (widget);
            target_slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (window));
        }

        if (target_slot != NULL)
        {
            GFile *location;

            location = nautilus_window_slot_get_location (target_slot);

            return g_file_get_uri (location);
        }
    }

    return NULL;
}

static void
slot_proxy_drag_data_received (GtkWidget        *widget,
                               GdkDrop          *drop,
                               GtkSelectionData *data,
                               gpointer          user_data)
{
    NautilusDragSlotProxyInfo *drag_info;
    GdkContentFormats *formats;
    g_autofree char *target_uri = NULL;
    GdkDragAction actions;

    drag_info = user_data;
    formats = gdk_drop_get_formats (drop);
    target_uri = get_target_uri (widget, drag_info);
    actions = 0;

    if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
    {
        drag_info->data.selection_list = nautilus_drag_build_selection_list (data);

        actions = nautilus_get_drop_actions_for_icons (target_uri, drag_info->data.selection_list);
    }
    else if (nautilus_content_formats_include_uri (formats))
    {
        g_auto (GStrv) uris = NULL;

        uris = gtk_selection_data_get_uris (data);
        drag_info->data.uri_list = nautilus_drag_uri_list_from_array (uris);

        actions = nautilus_get_drop_actions_for_uri (target_uri);
    }
    else if (nautilus_content_formats_include_text (formats))
    {
        actions = GDK_ACTION_COPY;
    }

    if (drag_info->drop_occurred)
    {
        slot_proxy_handle_drop (widget, drop, drag_info);
    }
    else
    {
        if (target_uri != NULL)
        {
            g_autoptr (NautilusFile) file = NULL;
            g_autoptr (NautilusDirectory) directory = NULL;
            gboolean can;

            file = nautilus_file_get_existing_by_uri (target_uri);
            directory = nautilus_directory_get_for_file (file);
            can = nautilus_file_can_write (file) && nautilus_directory_is_editable (directory);
            if (!can)
            {
                actions = 0;
            }
        }

        if (actions != 0)
        {
            gtk_drag_highlight (widget);
            slot_proxy_check_switch_location_timer (drag_info, widget);
        }
        else
        {
            gtk_drag_unhighlight (widget);
            slot_proxy_remove_switch_location_timer (drag_info);
        }

        drag_info->pending_actions = actions;

        gdk_drop_status (drop, actions);
    }
}

void
nautilus_drag_slot_proxy_init (GtkWidget          *widget,
                               NautilusFile       *target_file,
                               NautilusWindowSlot *target_slot)
{
    NautilusDragSlotProxyInfo *drag_info;

    const char *drop_types[] =
    {
        NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE,
    };
    g_autoptr (GdkContentFormats) targets = NULL;

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

    gtk_drag_dest_set (widget, 0,
                       NULL,
                       GDK_ACTION_MOVE |
                       GDK_ACTION_COPY |
                       GDK_ACTION_LINK |
                       GDK_ACTION_ASK);

    targets = gdk_content_formats_new (drop_types, G_N_ELEMENTS (drop_types));
    targets = gtk_content_formats_add_uri_targets (targets);
    targets = gtk_content_formats_add_text_targets (targets);
    gtk_drag_dest_set_target_list (widget, targets);

    g_signal_connect (widget, "drag-motion",
                      G_CALLBACK (slot_proxy_drag_motion),
                      drag_info);
    g_signal_connect (widget, "drag-drop",
                      G_CALLBACK (slot_proxy_drag_drop),
                      drag_info);
    g_signal_connect (widget, "drag-data-received",
                      G_CALLBACK (slot_proxy_drag_data_received),
                      drag_info);
    g_signal_connect (widget, "drag-leave",
                      G_CALLBACK (slot_proxy_drag_leave),
                      drag_info);
}
