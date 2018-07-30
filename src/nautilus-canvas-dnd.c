/* nautilus-canvas-dnd.c - Drag & drop handling for the canvas container widget.
 *
 *  Copyright (C) 1999, 2000 Free Software Foundation
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Ettore Perazzoli <ettore@gnu.org>,
 *           Darin Adler <darin@bentspoon.com>,
 *           Andy Hertzfeld <andy@eazel.com>
 *           Pavel Cisler <pavel@eazel.com>
 *
 *
 *  XDS support: Benedikt Meurer <benny@xfce.org> (adapted by Amos Brocco <amos.brocco@unifr.ch>)
 *
 */


#include <config.h>
#include <math.h>
#include <src/nautilus-window.h>

#include "nautilus-canvas-dnd.h"

#include "nautilus-canvas-container.h"
#include "nautilus-canvas-item.h"
#include "nautilus-canvas-private.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-selection-canvas-item.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nautilus-file-utilities.h"
#include "nautilus-file-changes-queue.h"
#include <stdio.h>
#include <string.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_CANVAS_CONTAINER
#include "nautilus-debug.h"

static const char *drag_types[] =
{
    NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE,
};

static const char *drop_types[] =
{
    NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE,
};
static void stop_dnd_highlight (GtkWidget *widget);

static GdkContentFormats *drop_types_list;

static char *nautilus_canvas_container_find_drop_target (NautilusCanvasContainer *container,
                                                         GdkDrop                 *drop,
                                                         int                      x,
                                                         int                      y,
                                                         gboolean                *icon_hit);

static EelCanvasItem *
create_selection_shadow (NautilusCanvasContainer *container,
                         GList                   *list)
{
    EelCanvasGroup *group;
    EelCanvas *canvas;
    int max_x, max_y;
    int min_x, min_y;
    GList *p;
    GtkAllocation allocation;

    if (list == NULL)
    {
        return NULL;
    }

    /* if we're only dragging a single item, don't worry about the shadow */
    if (list->next == NULL)
    {
        return NULL;
    }

    canvas = EEL_CANVAS (container);
    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);

    /* Creating a big set of rectangles in the canvas can be expensive, so
     *  we try to be smart and only create the maximum number of rectangles
     *  that we will need, in the vertical/horizontal directions.  */

    max_x = allocation.width;
    min_x = -max_x;

    max_y = allocation.height;
    min_y = -max_y;

    /* Create a group, so that it's easier to move all the items around at
     *  once.  */
    group = EEL_CANVAS_GROUP
                (eel_canvas_item_new (EEL_CANVAS_GROUP (canvas->root),
                                      eel_canvas_group_get_type (),
                                      NULL));

    for (p = list; p != NULL; p = p->next)
    {
        NautilusDragSelectionItem *item;
        int x1, y1, x2, y2;

        item = p->data;

        if (!item->got_icon_position)
        {
            continue;
        }

        x1 = item->icon_x;
        y1 = item->icon_y;
        x2 = x1 + item->icon_width;
        y2 = y1 + item->icon_height;

        eel_canvas_w2c (canvas, x1, y1, &x1, &y1);
        eel_canvas_w2c (canvas, x2, y2, &x2, &y2);

        if (x2 >= min_x && x1 <= max_x && y2 >= min_y && y1 <= max_y)
        {
            eel_canvas_item_new
                (group,
                NAUTILUS_TYPE_SELECTION_CANVAS_ITEM,
                "x1", (double) x1,
                "y1", (double) y1,
                "x2", (double) x2,
                "y2", (double) y2,
                NULL);
        }
    }

    return EEL_CANVAS_ITEM (group);
}

/* Source-side handling of the drag. */

/* iteration glue struct */
typedef struct
{
    gpointer iterator_context;
    NautilusDragEachSelectedItemDataGet iteratee;
    gpointer iteratee_data;
} CanvasGetDataBinderContext;

static void
canvas_widget_to_world (EelCanvas *canvas,
                        double     widget_x,
                        double     widget_y,
                        double    *world_x,
                        double    *world_y)
{
    eel_canvas_window_to_world (canvas,
                                widget_x + gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas))),
                                widget_y + gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas))),
                                world_x, world_y);
}

static gboolean
icon_get_data_binder (NautilusCanvasIcon *icon,
                      gpointer            data)
{
    CanvasGetDataBinderContext *context;
    EelDRect world_rect;
    g_autofree char *uri = NULL;
    NautilusCanvasContainer *container;
    g_autoptr (NautilusFile) file = NULL;

    context = (CanvasGetDataBinderContext *) data;

    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (context->iterator_context));

    container = NAUTILUS_CANVAS_CONTAINER (context->iterator_context);

    world_rect = nautilus_canvas_item_get_icon_rectangle (icon->item);

    uri = nautilus_canvas_container_get_icon_uri (container, icon);
    file = nautilus_file_get_by_uri (uri);
    g_free (uri);
    uri = nautilus_canvas_container_get_icon_activation_uri (container, icon);

    if (uri == NULL)
    {
        g_warning ("no URI for one of the iterated icons");
        return TRUE;
    }

    /* pass the uri, mouse-relative x/y and icon width/height */
    context->iteratee (uri,
                       world_rect.x0,
                       world_rect.y0,
                       world_rect.x1 - world_rect.x0,
                       world_rect.y1 - world_rect.y0,
                       context->iteratee_data);

    return TRUE;
}

/* Iterate over each selected icon in a NautilusCanvasContainer,
 * calling each_function on each.
 */
static void
nautilus_canvas_container_each_selected_icon (NautilusCanvasContainer *container,
                                              gboolean (*each_function)(NautilusCanvasIcon *, gpointer),
                                              gpointer data)
{
    GList *p;
    NautilusCanvasIcon *icon;

    for (p = container->details->icons; p != NULL; p = p->next)
    {
        icon = p->data;
        if (!icon->is_selected)
        {
            continue;
        }
        if (!each_function (icon, data))
        {
            return;
        }
    }
}

/* Adaptor function used with nautilus_canvas_container_each_selected_icon
 * to help iterate over all selected items, passing uris, x, y, w and h
 * values to the iteratee
 */
static void
each_icon_get_data_binder (NautilusDragEachSelectedItemDataGet iteratee,
                           gpointer                            iterator_context,
                           gpointer                            data)
{
    CanvasGetDataBinderContext context;
    NautilusCanvasContainer *container;

    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (iterator_context));
    container = NAUTILUS_CANVAS_CONTAINER (iterator_context);

    context.iterator_context = iterator_context;
    context.iteratee = iteratee;
    context.iteratee_data = data;
    nautilus_canvas_container_each_selected_icon (container, icon_get_data_binder, &context);
}

/* Called when the data for drag&drop is needed */
static void
drag_data_get_callback (GtkWidget        *widget,
                        GdkDrag          *context,
                        GtkSelectionData *selection_data,
                        gpointer          data)
{
    NautilusDragInfo *drag_info;

    g_assert (widget != NULL);
    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (widget));
    g_return_if_fail (context != NULL);

    /* Call common function from nautilus-drag that set's up
     * the selection data in the right format. Pass it means to
     * iterate all the selected icons.
     */
    drag_info = &(NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info->drag_info);
    nautilus_drag_drag_data_get_from_cache (drag_info->selection_cache, selection_data);
}


/* Target-side handling of the drag.  */

static void
nautilus_canvas_container_position_shadow (NautilusCanvasContainer *container,
                                           int                      x,
                                           int                      y)
{
    EelCanvasItem *shadow;

    shadow = container->details->dnd_info->shadow;
    if (shadow == NULL)
    {
        return;
    }

    x -= container->details->dnd_info->drag_info.start_x;
    y -= container->details->dnd_info->drag_info.start_y;

    /* This is used as an offset when drawing the selection item.
     * Offsetting by the position of the cursor would be a bit too much,
     * so we only take the delta from the start position.
     */
    eel_canvas_item_set (shadow, "x", (double) x, "y", (double) y, NULL);
    eel_canvas_item_show (shadow);
}

static void
stop_cache_selection_list (NautilusDragInfo *drag_info)
{
    if (drag_info->file_list_info_handler)
    {
        nautilus_file_list_cancel_call_when_ready (drag_info->file_list_info_handler);
        drag_info->file_list_info_handler = NULL;
    }
}

static void
cache_selection_list (NautilusDragInfo *drag_info)
{
    GList *files;

    files = nautilus_drag_file_list_from_selection_list (drag_info->selection_list);
    nautilus_file_list_call_when_ready (files,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        drag_info->file_list_info_handler,
                                        NULL, NULL);

    g_list_free_full (files, g_object_unref);
}

static void
nautilus_canvas_container_dropped_canvas_feedback (GtkWidget        *widget,
                                                   GtkSelectionData *data,
                                                   int               x,
                                                   int               y)
{
    NautilusCanvasContainer *container;
    NautilusCanvasDndInfo *dnd_info;

    container = NAUTILUS_CANVAS_CONTAINER (widget);
    dnd_info = container->details->dnd_info;

    /* Delete old selection list. */
    stop_cache_selection_list (&dnd_info->drag_info);
    nautilus_drag_destroy_selection_list (dnd_info->drag_info.selection_list);
    dnd_info->drag_info.selection_list = NULL;

    /* Delete old shadow if any. */
    if (dnd_info->shadow != NULL)
    {
        /* FIXME bugzilla.gnome.org 42484:
         * Is a destroy really sufficient here? Who does the unref? */
        eel_canvas_item_destroy (dnd_info->shadow);
    }

    /* Build the selection list and the shadow. */
    dnd_info->drag_info.selection_list = nautilus_drag_build_selection_list (data);
    cache_selection_list (&dnd_info->drag_info);
    dnd_info->shadow = create_selection_shadow (container, dnd_info->drag_info.selection_list);
    nautilus_canvas_container_position_shadow (container, x, y);
}

/* FIXME bugzilla.gnome.org 47445: Needs to become a shared function */
static void
get_data_on_first_target_we_support (GtkWidget *widget,
                                     GdkDrop   *drop,
                                     int        x,
                                     int        y)
{
    GdkContentFormats *list;
    GdkAtom target;

    if (drop_types_list == NULL)
    {
        drop_types_list = gdk_content_formats_new (drop_types, G_N_ELEMENTS (drop_types));
        drop_types_list = gtk_content_formats_add_text_targets (drop_types_list);
        drop_types_list = gtk_content_formats_add_uri_targets (drop_types_list);
    }

    list = drop_types_list;
    target = gtk_drag_dest_find_target (widget, drop, list);
    if (target != NULL)
    {
        gboolean found;

        found = gdk_content_formats_contain_mime_type (list, target);
        g_assert (found);

        gtk_drag_get_data (GTK_WIDGET (widget), drop, target);
    }
}

static void
nautilus_canvas_container_ensure_drag_data (NautilusCanvasContainer *container,
                                            GdkDrop                 *drop)
{
    if (container->details->dnd_info->drag_info.selection_data == NULL)
    {
        get_data_on_first_target_we_support (GTK_WIDGET (container), drop, 0, 0);
    }
}

static void
remove_hover_timer (NautilusCanvasDndInfo *dnd_info)
{
    if (dnd_info->hover_id != 0)
    {
        g_source_remove (dnd_info->hover_id);
        dnd_info->hover_id = 0;
    }
}

static void
nautilus_canvas_container_free_drag_data (NautilusCanvasContainer *container)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = container->details->dnd_info;

    g_clear_pointer (&dnd_info->shadow, eel_canvas_item_destroy);
    g_clear_pointer (&dnd_info->drag_info.selection_data, gtk_selection_data_free);
    g_clear_pointer (&dnd_info->target_uri, g_free);

    remove_hover_timer (dnd_info);
}

static void
drag_end_callback (GtkWidget *widget,
                   GdkDrag   *context,
                   gpointer   data)
{
    NautilusCanvasContainer *container;
    NautilusCanvasDndInfo *dnd_info;
    NautilusWindow *window;

    container = NAUTILUS_CANVAS_CONTAINER (widget);
    window = NAUTILUS_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (container)));
    dnd_info = container->details->dnd_info;

    stop_cache_selection_list (&dnd_info->drag_info);
    nautilus_drag_destroy_selection_list (dnd_info->drag_info.selection_list);
    nautilus_drag_destroy_selection_list (dnd_info->drag_info.selection_cache);
    nautilus_drag_destroy_selection_list (container->details->dnd_source_info->selection_cache);
    dnd_info->drag_info.selection_list = NULL;
    dnd_info->drag_info.selection_cache = NULL;
    container->details->dnd_source_info->selection_cache = NULL;

    nautilus_window_end_dnd (window, context);

    nautilus_canvas_container_free_drag_data (container);
}

static NautilusCanvasIcon *
nautilus_canvas_container_item_at (NautilusCanvasContainer *container,
                                   int                      x,
                                   int                      y)
{
    GList *p;
    int size;
    EelIRect point;

    /* build the hit-test rectangle. Base the size on the scale factor to ensure that it is
     * non-empty even at the smallest scale factor
     */

    size = MAX (1, 1 + (1 / EEL_CANVAS (container)->pixels_per_unit));
    point.x0 = x;
    point.y0 = y;
    point.x1 = x + size;
    point.y1 = y + size;

    for (p = container->details->icons; p != NULL; p = p->next)
    {
        NautilusCanvasIcon *icon;
        icon = p->data;

        if (nautilus_canvas_item_hit_test_rectangle (icon->item, point))
        {
            return icon;
        }
    }

    return NULL;
}

static char *
get_container_uri (NautilusCanvasContainer *container)
{
    char *uri;

    /* get the URI associated with the container */
    uri = NULL;
    g_signal_emit_by_name (container, "get-container-uri", &uri);
    return uri;
}

static gboolean
nautilus_canvas_container_selection_items_local (NautilusCanvasContainer *container,
                                                 GList                   *items)
{
    char *container_uri_string;
    gboolean result;

    /* must have at least one item */
    g_assert (items);

    /* get the URI associated with the container */
    container_uri_string = get_container_uri (container);

    result = nautilus_drag_items_local (container_uri_string, items);

    g_free (container_uri_string);

    return result;
}

/* handle dropped uri list */
static void
receive_dropped_uri_list (NautilusCanvasContainer *container,
                          const char              *uri_list,
                          GdkDrop                 *drop,
                          int                      x,
                          int                      y)
{
    char *drop_target;

    if (uri_list == NULL)
    {
        return;
    }

    drop_target = nautilus_canvas_container_find_drop_target (container, drop, x, y, NULL);

    g_signal_emit_by_name (container, "handle-uri-list",
                           uri_list,
                           drop_target,
                           gdk_drop_get_actions (drop));

    g_free (drop_target);
}

/* handle dropped text */
static void
receive_dropped_text (NautilusCanvasContainer *container,
                      const char              *text,
                      GdkDrop                 *drop,
                      int                      x,
                      int                      y)
{
    char *drop_target;

    if (text == NULL)
    {
        return;
    }

    drop_target = nautilus_canvas_container_find_drop_target (container, drop, x, y, NULL);

    g_signal_emit_by_name (container, "handle-text",
                           text,
                           drop_target,
                           gdk_drop_get_actions (drop));

    g_free (drop_target);
}

static int
auto_scroll_timeout_callback (gpointer data)
{
    NautilusCanvasContainer *container;
    GtkWidget *widget;
    float x_scroll_delta, y_scroll_delta;

    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (data));
    widget = GTK_WIDGET (data);
    container = NAUTILUS_CANVAS_CONTAINER (widget);

    if (container->details->dnd_info->drag_info.waiting_to_autoscroll
        && container->details->dnd_info->drag_info.start_auto_scroll_in > g_get_monotonic_time ())
    {
        /* not yet */
        return TRUE;
    }

    container->details->dnd_info->drag_info.waiting_to_autoscroll = FALSE;

    nautilus_drag_autoscroll_calculate_delta (widget, &x_scroll_delta, &y_scroll_delta);
    if (x_scroll_delta == 0 && y_scroll_delta == 0)
    {
        /* no work */
        return TRUE;
    }

    /* Clear the old dnd highlight frame */
    gtk_widget_queue_draw (widget);

    if (!nautilus_canvas_container_scroll (container, (int) x_scroll_delta, (int) y_scroll_delta))
    {
        /* the scroll value got pinned to a min or max adjustment value,
         * we ended up not scrolling
         */
        return TRUE;
    }

    /* update cached drag start offsets */
    container->details->dnd_info->drag_info.start_x -= x_scroll_delta;
    container->details->dnd_info->drag_info.start_y -= y_scroll_delta;

    gtk_widget_queue_draw (widget);

    return TRUE;
}

static void
set_up_auto_scroll_if_needed (NautilusCanvasContainer *container)
{
    nautilus_drag_autoscroll_start (&container->details->dnd_info->drag_info,
                                    GTK_WIDGET (container),
                                    auto_scroll_timeout_callback,
                                    container);
}

static void
stop_auto_scroll (NautilusCanvasContainer *container)
{
    nautilus_drag_autoscroll_stop (&container->details->dnd_info->drag_info);
}

static void
handle_nonlocal_move (NautilusCanvasContainer *container,
                      GdkDragAction            action,
                      const char              *target_uri,
                      gboolean                 icon_hit)
{
    GList *source_uris, *p;
    gboolean free_target_uri;

    if (container->details->dnd_info->drag_info.selection_list == NULL)
    {
        return;
    }

    source_uris = NULL;
    for (p = container->details->dnd_info->drag_info.selection_list; p != NULL; p = p->next)
    {
        /* do a shallow copy of all the uri strings of the copied files */
        source_uris = g_list_prepend (source_uris, ((NautilusDragSelectionItem *) p->data)->uri);
    }
    source_uris = g_list_reverse (source_uris);

    free_target_uri = FALSE;

    /* start the copy */
    g_signal_emit_by_name (container, "move-copy-items",
                           source_uris,
                           target_uri,
                           action);

    if (free_target_uri)
    {
        g_free ((char *) target_uri);
    }

    g_list_free (source_uris);
}

static char *
nautilus_canvas_container_find_drop_target (NautilusCanvasContainer *container,
                                            GdkDrop                 *drop,
                                            int                      x,
                                            int                      y,
                                            gboolean                *icon_hit)
{
    NautilusCanvasIcon *drop_target_icon;
    char *container_uri;

    if (icon_hit)
    {
        *icon_hit = FALSE;
    }

    if (container->details->dnd_info->drag_info.selection_data == NULL)
    {
        return NULL;
    }

    /* FIXME bugzilla.gnome.org 42485:
     * These "can_accept_items" tests need to be done by
     * the canvas view, not here. This file is not supposed to know
     * that the target is a file.
     */

    /* Find the item we hit with our drop, if any */
    drop_target_icon = nautilus_canvas_container_item_at (container, x, y);
    if (drop_target_icon != NULL)
    {
        g_autofree char *icon_uri = NULL;

        icon_uri = nautilus_canvas_container_get_icon_uri (container, drop_target_icon);
        if (icon_uri != NULL)
        {
            g_autoptr (NautilusFile) file = NULL;

            file = nautilus_file_get_by_uri (icon_uri);

            if (!nautilus_drag_can_accept_data (file,
                                                drop,
                                                container->details->dnd_info->drag_info.selection_list))
            {
                /* the item we dropped our selection on cannot accept the items,
                 * do the same thing as if we just dropped the items on the canvas
                 */
                drop_target_icon = NULL;
            }
        }
    }

    if (drop_target_icon == NULL)
    {
        if (icon_hit)
        {
            *icon_hit = FALSE;
        }

        container_uri = get_container_uri (container);

        if (container_uri != NULL)
        {
            g_autoptr (NautilusFile) file = NULL;
            gboolean can;

            file = nautilus_file_get_by_uri (container_uri);
            can = nautilus_drag_can_accept_data (file,
                                                 drop,
                                                 container->details->dnd_info->drag_info.selection_list);
            if (!can)
            {
                g_clear_pointer (&container_uri, g_free);
            }
        }

        return container_uri;
    }

    if (icon_hit)
    {
        *icon_hit = TRUE;
    }
    return nautilus_canvas_container_get_icon_drop_target_uri (container, drop_target_icon);
}

static void
nautilus_canvas_container_receive_dropped_icons (NautilusCanvasContainer *container,
                                                 GdkDrop                 *drop,
                                                 int                      x,
                                                 int                      y)
{
    char *drop_target;
    gboolean local_move_only;
    double world_x, world_y;
    gboolean icon_hit;
    GdkDragAction action, real_action;

    drop_target = NULL;

    if (container->details->dnd_info->drag_info.selection_list == NULL)
    {
        return;
    }

    real_action = gdk_drop_get_actions (drop);

    if (!gdk_drag_action_is_unique (real_action))
    {
        action = GDK_ACTION_ALL;
        real_action = nautilus_drag_drop_action_ask (GTK_WIDGET (container), action);
    }

    if (real_action > 0)
    {
        eel_canvas_window_to_world (EEL_CANVAS (container),
                                    x + gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container))),
                                    y + gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container))),
                                    &world_x, &world_y);

        drop_target = nautilus_canvas_container_find_drop_target (container,
                                                                  drop, x, y, &icon_hit);

        local_move_only = FALSE;
        if (!icon_hit && real_action == GDK_ACTION_MOVE)
        {
            local_move_only = nautilus_canvas_container_selection_items_local
                                  (container, container->details->dnd_info->drag_info.selection_list);
        }

        /* If the move is local, there is nothing to do. */
        if (!local_move_only)
        {
            handle_nonlocal_move (container, real_action, drop_target, icon_hit);
        }
    }

    g_free (drop_target);
    stop_cache_selection_list (&container->details->dnd_info->drag_info);
    nautilus_drag_destroy_selection_list (container->details->dnd_info->drag_info.selection_list);
    container->details->dnd_info->drag_info.selection_list = NULL;
}

NautilusDragInfo *
nautilus_canvas_dnd_get_drag_source_data (NautilusCanvasContainer *container)
{
    return container->details->dnd_source_info;
}

static GdkDragAction
nautilus_canvas_container_get_drop_actions (NautilusCanvasContainer *container,
                                            GdkDrop                 *drop,
                                            int                      x,
                                            int                      y)
{
    GtkSelectionData *data;
    g_autofree char *drop_target = NULL;
    gboolean icon_hit;
    double world_x, world_y;
    GdkAtom target;

    if (container->details->dnd_info->drag_info.selection_data == NULL)
    {
        /* drag_data_received_callback didn't get called yet */
        return 0;
    }

    data = container->details->dnd_info->drag_info.selection_data;
    icon_hit = FALSE;

    /* find out if we're over an canvas */
    canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);

    drop_target = nautilus_canvas_container_find_drop_target (container,
                                                              drop, x, y, &icon_hit);
    if (drop_target == NULL)
    {
        return 0;
    }

    target = gtk_selection_data_get_target (data);

    if (target == g_intern_static_string (NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
    {
        if (container->details->dnd_info->drag_info.selection_list != NULL)
        {
            return nautilus_get_drop_actions_for_icons (drop_target,
                                                        container->details->dnd_info->drag_info.selection_list);
        }
    }
    else if (gtk_selection_data_targets_include_uri (data))
    {
        return nautilus_get_drop_actions_for_uri (drop_target);
    }
    else if (gtk_selection_data_targets_include_text (data))
    {
        return GDK_ACTION_COPY;
    }

    return 0;
}

static void
set_drop_target (NautilusCanvasContainer *container,
                 NautilusCanvasIcon      *icon)
{
    NautilusCanvasIcon *old_icon;

    /* Check if current drop target changed, update icon drop
     * higlight if needed.
     */
    old_icon = container->details->drop_target;
    if (icon == old_icon)
    {
        return;
    }

    /* Remember the new drop target for the next round. */
    container->details->drop_target = icon;
    nautilus_canvas_container_update_icon (container, old_icon);
    nautilus_canvas_container_update_icon (container, icon);
}

static void
nautilus_canvas_dnd_update_drop_target (NautilusCanvasContainer *container,
                                        GdkDrop                 *drop,
                                        int                      x,
                                        int                      y)
{
    NautilusCanvasIcon *icon;

    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));

    /* Find the item we hit with our drop, if any. */
    icon = nautilus_canvas_container_item_at (container, x, y);

    /* FIXME bugzilla.gnome.org 42485:
     * These "can_accept_items" tests need to be done by
     * the canvas view, not here. This file is not supposed to know
     * that the target is a file.
     */

    /* Find if target canvas accepts our drop. */
    if (icon != NULL)
    {
        g_autofree char *uri = NULL;
        g_autoptr (NautilusFile) file = NULL;

        uri = nautilus_canvas_container_get_icon_uri (container, icon);
        file = nautilus_file_get_by_uri (uri);

        if (!nautilus_drag_can_accept_data (file,
                                            drop,
                                            container->details->dnd_info->drag_info.selection_list))
        {
            icon = NULL;
        }
    }

    set_drop_target (container, icon);
}

static void
drag_leave_callback (GtkWidget *widget,
                     GdkDrop   *drop,
                     gpointer   data)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    if (dnd_info->shadow != NULL)
    {
        eel_canvas_item_hide (dnd_info->shadow);
    }

    stop_dnd_highlight (widget);

    set_drop_target (NAUTILUS_CANVAS_CONTAINER (widget), NULL);
    stop_auto_scroll (NAUTILUS_CANVAS_CONTAINER (widget));
    nautilus_canvas_container_free_drag_data (NAUTILUS_CANVAS_CONTAINER (widget));
}

static void
drag_begin_callback (GtkWidget *widget,
                     GdkDrag   *context,
                     gpointer   data)
{
    NautilusCanvasContainer *container;
    NautilusDragInfo *drag_info;
    g_autoptr (GdkPaintable) paintable = NULL;
    double x1, y1, x2, y2, winx, winy;
    int x_offset, y_offset;
    int start_x, start_y;
    GList *dragged_files;

    container = NAUTILUS_CANVAS_CONTAINER (widget);

    start_x = container->details->dnd_info->drag_info.start_x +
              gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));
    start_y = container->details->dnd_info->drag_info.start_y +
              gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container)));

    container->details->dnd_info->x = start_x;
    container->details->dnd_info->y = start_y;

    paintable = nautilus_canvas_item_get_drag_paintable (container->details->drag_icon->item);

    /* compute the image's offset */
    eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (container->details->drag_icon->item),
                                &x1, &y1, &x2, &y2);
    eel_canvas_world_to_window (EEL_CANVAS (container),
                                x1, y1, &winx, &winy);
    x_offset = start_x - winx;
    y_offset = start_y - winy;

    gtk_drag_set_icon_paintable (context, paintable, x_offset, y_offset);

    /* cache the data at the beginning since the view may change */
    drag_info = &(container->details->dnd_info->drag_info);
    drag_info->selection_cache = nautilus_drag_create_selection_cache (widget,
                                                                       each_icon_get_data_binder);

    container->details->dnd_source_info->selection_cache = nautilus_drag_create_selection_cache (widget,
                                                                                                 each_icon_get_data_binder);

    dragged_files = nautilus_drag_file_list_from_selection_list (drag_info->selection_cache);
    if (nautilus_file_list_are_all_folders (dragged_files))
    {
        GtkWidget *toplevel;
        NautilusWindow *window;

        toplevel = gtk_widget_get_toplevel (widget);
        window = NAUTILUS_WINDOW (toplevel);

        nautilus_window_start_dnd (window, context);
    }
    g_list_free_full (dragged_files, g_object_unref);
}

void
nautilus_canvas_dnd_begin_drag (NautilusCanvasContainer *container,
                                GdkDragAction            actions,
                                int                      start_x,
                                int                      start_y)
{
    NautilusCanvasDndInfo *dnd_info;
    NautilusDragInfo *dnd_source_info;

    g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container));

    dnd_info = container->details->dnd_info;
    container->details->dnd_source_info = g_new0 (NautilusDragInfo, 1);
    dnd_source_info = container->details->dnd_source_info;
    g_return_if_fail (dnd_info != NULL);

    /* Notice that the event is in bin_window coordinates, because of
     *  the way the canvas handles events.
     */
    dnd_info->drag_info.start_x = start_x -
                                  gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));
    dnd_info->drag_info.start_y = start_y -
                                  gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container)));

    dnd_source_info->source_actions = actions;
    /* start the drag */
    gtk_drag_begin (GTK_WIDGET (container),
                    NULL,
                    dnd_info->drag_info.formats,
                    actions,
                    dnd_info->drag_info.start_x,
                    dnd_info->drag_info.start_y);
}

static void
start_dnd_highlight (GtkWidget *widget)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    if (!dnd_info->highlighted)
    {
        dnd_info->highlighted = TRUE;
        gtk_widget_queue_draw (widget);
    }
}

static void
stop_dnd_highlight (GtkWidget *widget)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    if (dnd_info->highlighted)
    {
        dnd_info->highlighted = FALSE;
        gtk_widget_queue_draw (widget);
    }
}

static gboolean
hover_timer (gpointer user_data)
{
    NautilusCanvasContainer *container = user_data;
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = container->details->dnd_info;

    dnd_info->hover_id = 0;

    g_signal_emit_by_name (container, "handle-hover", dnd_info->target_uri);

    return G_SOURCE_REMOVE;
}

static void
check_hover_timer (NautilusCanvasContainer *container,
                   const char              *uri)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = container->details->dnd_info;

    if (g_strcmp0 (uri, dnd_info->target_uri) == 0)
    {
        return;
    }

    remove_hover_timer (dnd_info);

    g_clear_pointer (&dnd_info->target_uri, g_free);

    if (uri != NULL)
    {
        dnd_info->target_uri = g_strdup (uri);
        dnd_info->hover_id = g_timeout_add (HOVER_TIMEOUT, hover_timer, container);
    }
}

static gboolean
drag_motion_callback (GtkWidget *widget,
                      GdkDrop   *drop,
                      int        x,
                      int        y)
{
    NautilusCanvasContainer *container;
    GdkDragAction action;

    container = NAUTILUS_CANVAS_CONTAINER (widget);

    container->details->dnd_info->x = x;
    container->details->dnd_info->y = y;

    nautilus_canvas_container_ensure_drag_data (container, drop);
    nautilus_canvas_container_position_shadow (container, x, y);
    nautilus_canvas_dnd_update_drop_target (container, drop, x, y);
    set_up_auto_scroll_if_needed (container);
    /* Find out what the drop actions are based on our drag selection and
     * the drop target.
     */
    action = nautilus_canvas_container_get_drop_actions (container, drop, x, y);
    if (action != 0)
    {
        g_autofree char *uri = NULL;

        uri = nautilus_canvas_container_find_drop_target (container, drop, x, y, NULL);

        check_hover_timer (container, uri);
        start_dnd_highlight (widget);
    }
    else
    {
        remove_hover_timer (container->details->dnd_info);
    }

    gdk_drop_status (drop, action);

    return TRUE;
}

static gboolean
drag_drop_callback (GtkWidget *widget,
                    GdkDrop   *drop,
                    int        x,
                    int        y,
                    gpointer   data)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    /* tell the drag_data_received callback that
     *  the drop occurred and that it can actually
     *  process the actions.
     *  make sure it is going to be called at least once.
     */
    dnd_info->drag_info.drop_occurred = TRUE;
    dnd_info->x = x;
    dnd_info->y = y;

    get_data_on_first_target_we_support (widget, drop, x, y);

    return TRUE;
}

void
nautilus_canvas_dnd_end_drag (NautilusCanvasContainer *container)
{
    NautilusCanvasDndInfo *dnd_info;

    g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container));

    dnd_info = container->details->dnd_info;
    g_return_if_fail (dnd_info != NULL);
    stop_auto_scroll (container);
    /* Do nothing.
     * Can that possibly be right?
     */
}

/** this callback is called in 2 cases.
 *   It is called upon drag_motion events to get the actual data
 *   In that case, it just makes sure it gets the data.
 *   It is called upon drop_drop events to execute the actual
 *   actions on the received action. In that case, it actually first makes sure
 *   that we have got the data then processes it.
 */

static void
drag_data_received_callback (GtkWidget        *widget,
                             GdkDrop          *drop,
                             GtkSelectionData *data,
                             gpointer          user_data)
{
    NautilusCanvasContainer *container;
    NautilusCanvasDndInfo *dnd_info;
    NautilusDragInfo *drag_info;
    GdkContentFormats *formats;

    container = NAUTILUS_CANVAS_CONTAINER (widget);
    dnd_info = container->details->dnd_info;
    drag_info = &dnd_info->drag_info;
    formats = gdk_drop_get_formats (drop);

    g_clear_pointer (&drag_info->selection_data, gtk_selection_data_free);
    drag_info->selection_data = gtk_selection_data_copy (data);

    if (gdk_content_formats_contain_mime_type (formats,
                                               NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
    {
        nautilus_canvas_container_dropped_canvas_feedback (widget,
                                                           data,
                                                           dnd_info->x,
                                                           dnd_info->y);
    }

    /* this is the second use case of this callback.
     * we have to do the actual work for the drop.
     */
    if (drag_info->drop_occurred)
    {
        gboolean success;
        GdkDragAction action;

        success = FALSE;
        action = 0;

        if (gtk_selection_data_targets_include_text (data))
        {
            g_autofree unsigned char *text = NULL;

            text = gtk_selection_data_get_text (data);

            receive_dropped_text (container,
                                  (char *) text,
                                  drop,
                                  dnd_info->x,
                                  dnd_info->y);
            success = TRUE;
        }
        else if (gtk_selection_data_targets_include_uri (data))
        {
            receive_dropped_uri_list (container,
                                      (char *) gtk_selection_data_get_data (data),
                                      drop,
                                      dnd_info->x,
                                      dnd_info->y);
            success = TRUE;
        }
        else if (gdk_content_formats_contain_mime_type (formats,
                                                        NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
        {
            nautilus_canvas_container_receive_dropped_icons (container,
                                                             drop,
                                                             dnd_info->x,
                                                             dnd_info->y);
        }

        if (success)
        {
            action = gdk_drop_get_actions (drop);
        }

        gdk_drop_finish (drop, action);

        nautilus_canvas_container_free_drag_data (container);

        set_drop_target (container, NULL);

        /* reinitialise it for the next dnd */
        drag_info->drop_occurred = FALSE;
    }
}

void
nautilus_canvas_dnd_init (NautilusCanvasContainer *container)
{
    NautilusCanvasDndInfo *dnd_info;
    g_autoptr (GdkContentFormats) targets = NULL;

    g_return_if_fail (container != NULL);
    g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container));

    container->details->dnd_info = g_new0 (NautilusCanvasDndInfo, 1);

    dnd_info = container->details->dnd_info;

    dnd_info->drag_info.formats = gdk_content_formats_new (drag_types, G_N_ELEMENTS (drag_types));
    dnd_info->drag_info.formats = gtk_content_formats_add_uri_targets (dnd_info->drag_info.formats);

    /* Set up the widget as a drag destination.
     * (But not a source, as drags starting from this widget will be
     * implemented by dealing with events manually.)
     */
    targets = gdk_content_formats_new (drop_types, G_N_ELEMENTS (drop_types));
    targets = gtk_content_formats_add_uri_targets (targets);

    gtk_drag_dest_set (GTK_WIDGET (container),
                       0,
                       targets,
                       GDK_ACTION_ALL);


    /* Messages for outgoing drag. */
    g_signal_connect (container, "drag-begin",
                      G_CALLBACK (drag_begin_callback), NULL);
    g_signal_connect (container, "drag-data-get",
                      G_CALLBACK (drag_data_get_callback), NULL);
    g_signal_connect (container, "drag-end",
                      G_CALLBACK (drag_end_callback), NULL);

    /* Messages for incoming drag. */
    g_signal_connect (container, "drag-data-received",
                      G_CALLBACK (drag_data_received_callback), NULL);
    g_signal_connect (container, "drag-motion",
                      G_CALLBACK (drag_motion_callback), NULL);
    g_signal_connect (container, "drag-drop",
                      G_CALLBACK (drag_drop_callback), NULL);
    g_signal_connect (container, "drag-leave",
                      G_CALLBACK (drag_leave_callback), NULL);
}

void
nautilus_canvas_dnd_fini (NautilusCanvasContainer *container)
{
    g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container));

    if (container->details->dnd_info != NULL)
    {
        stop_auto_scroll (container);

        nautilus_drag_finalize (&container->details->dnd_info->drag_info);
        container->details->dnd_info = NULL;
    }
}
