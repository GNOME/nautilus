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
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nautilus-file-utilities.h"
#include "nautilus-file-changes-queue.h"
#include <stdio.h>
#include <string.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_CANVAS_CONTAINER
#include "nautilus-debug.h"

static const GtkTargetEntry drag_types [] =
{
    { NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
    { NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
};

static const GtkTargetEntry drop_types [] =
{
    { NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
    /* prefer "_NETSCAPE_URL" over "text/uri-list" to satisfy web browsers. */
    { NAUTILUS_ICON_DND_NETSCAPE_URL_TYPE, 0, NAUTILUS_ICON_DND_NETSCAPE_URL },
    /* prefer XDS over "text/uri-list" */
    { NAUTILUS_ICON_DND_XDNDDIRECTSAVE_TYPE, 0, NAUTILUS_ICON_DND_XDNDDIRECTSAVE },     /* XDS Protocol Type */
    { NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
    { NAUTILUS_ICON_DND_RAW_TYPE, 0, NAUTILUS_ICON_DND_RAW },
    /* Must be last: */
    { NAUTILUS_ICON_DND_ROOTWINDOW_DROP_TYPE, 0, NAUTILUS_ICON_DND_ROOTWINDOW_DROP }
};
static void     stop_dnd_highlight (GtkWidget *widget);
static void     dnd_highlight_queue_redraw (GtkWidget *widget);

static GtkTargetList *drop_types_list = NULL;
static GtkTargetList *drop_types_list_root = NULL;

static char *nautilus_canvas_container_find_drop_target (NautilusCanvasContainer *container,
                                                         GdkDragContext          *context,
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

/* Set the affine instead of the x and y position.
 * Simple, and setting x and y was broken at one point.
 */
static void
set_shadow_position (EelCanvasItem *shadow,
                     double         x,
                     double         y)
{
    eel_canvas_item_set (shadow,
                         "x", x, "y", y,
                         NULL);
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
canvas_rect_world_to_widget (EelCanvas *canvas,
                             EelDRect  *world_rect,
                             EelIRect  *widget_rect)
{
    EelDRect window_rect;
    GtkAdjustment *hadj, *vadj;

    hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas));
    vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas));

    eel_canvas_world_to_window (canvas,
                                world_rect->x0, world_rect->y0,
                                &window_rect.x0, &window_rect.y0);
    eel_canvas_world_to_window (canvas,
                                world_rect->x1, world_rect->y1,
                                &window_rect.x1, &window_rect.y1);
    widget_rect->x0 = (int) window_rect.x0 - gtk_adjustment_get_value (hadj);
    widget_rect->y0 = (int) window_rect.y0 - gtk_adjustment_get_value (vadj);
    widget_rect->x1 = (int) window_rect.x1 - gtk_adjustment_get_value (hadj);
    widget_rect->y1 = (int) window_rect.y1 - gtk_adjustment_get_value (vadj);
}

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
    EelIRect widget_rect;
    char *uri;
    NautilusCanvasContainer *container;
    NautilusFile *file;

    context = (CanvasGetDataBinderContext *) data;

    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (context->iterator_context));

    container = NAUTILUS_CANVAS_CONTAINER (context->iterator_context);

    world_rect = nautilus_canvas_item_get_icon_rectangle (icon->item);

    canvas_rect_world_to_widget (EEL_CANVAS (container), &world_rect, &widget_rect);

    uri = nautilus_canvas_container_get_icon_uri (container, icon);
    file = nautilus_file_get_by_uri (uri);
    g_free (uri);
    uri = nautilus_canvas_container_get_icon_activation_uri (container, icon);

    if (uri == NULL)
    {
        g_warning ("no URI for one of the iterated icons");
        nautilus_file_unref (file);
        return TRUE;
    }

    widget_rect = eel_irect_offset_by (widget_rect,
                                       -container->details->dnd_info->drag_info.start_x,
                                       -container->details->dnd_info->drag_info.start_y);

    widget_rect = eel_irect_scale_by (widget_rect,
                                      1 / EEL_CANVAS (container)->pixels_per_unit);

    /* pass the uri, mouse-relative x/y and icon width/height */
    context->iteratee (uri,
                       (int) widget_rect.x0,
                       (int) widget_rect.y0,
                       widget_rect.x1 - widget_rect.x0,
                       widget_rect.y1 - widget_rect.y0,
                       context->iteratee_data);

    g_free (uri);
    nautilus_file_unref (file);

    return TRUE;
}

typedef gboolean (*CanvasContainerEachFunc)(NautilusCanvasIcon *,
                                            gpointer);

/* Iterate over each selected icon in a NautilusCanvasContainer,
 * calling each_function on each.
 */
static void
nautilus_canvas_container_each_selected_icon (NautilusCanvasContainer *container,
                                              CanvasContainerEachFunc  each_function,
                                              gpointer                 data)
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
                        GdkDragContext   *context,
                        GtkSelectionData *selection_data,
                        guint             info,
                        guint32           time,
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
    nautilus_drag_drag_data_get_from_cache (drag_info->selection_cache, context, selection_data, info, time);
}


/* Target-side handling of the drag.  */

static void
nautilus_canvas_container_position_shadow (NautilusCanvasContainer *container,
                                           int                      x,
                                           int                      y)
{
    EelCanvasItem *shadow;
    double world_x, world_y;

    shadow = container->details->dnd_info->shadow;
    if (shadow == NULL)
    {
        return;
    }

    canvas_widget_to_world (EEL_CANVAS (container), x, y,
                            &world_x, &world_y);

    set_shadow_position (shadow, world_x, world_y);
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

static char *
get_direct_save_filename (GdkDragContext *context)
{
    guchar *prop_text;
    gint prop_len;

    if (!gdk_property_get (gdk_drag_context_get_source_window (context), gdk_atom_intern (NAUTILUS_ICON_DND_XDNDDIRECTSAVE_TYPE, FALSE),
                           gdk_atom_intern ("text/plain", FALSE), 0, 1024, FALSE, NULL, NULL,
                           &prop_len, &prop_text))
    {
        return NULL;
    }

    /* Zero-terminate the string */
    prop_text = g_realloc (prop_text, prop_len + 1);
    prop_text[prop_len] = '\0';

    /* Verify that the file name provided by the source is valid */
    if (*prop_text == '\0' ||
        strchr ((const gchar *) prop_text, G_DIR_SEPARATOR) != NULL)
    {
        DEBUG ("Invalid filename provided by XDS drag site");
        g_free (prop_text);
        return NULL;
    }

    return (gchar *) prop_text;
}

static void
set_direct_save_uri (GtkWidget        *widget,
                     GdkDragContext   *context,
                     NautilusDragInfo *drag_info,
                     int               x,
                     int               y)
{
    GFile *base, *child;
    char *filename, *drop_target;
    gchar *uri;

    drag_info->got_drop_data_type = TRUE;
    drag_info->data_type = NAUTILUS_ICON_DND_XDNDDIRECTSAVE;

    uri = NULL;

    filename = get_direct_save_filename (context);
    drop_target = nautilus_canvas_container_find_drop_target (NAUTILUS_CANVAS_CONTAINER (widget),
                                                              context, x, y, NULL);

    if (drop_target && eel_uri_is_trash (drop_target))
    {
        g_free (drop_target);
        drop_target = NULL;         /* Cannot save to trash ...*/
    }

    if (filename != NULL && drop_target != NULL)
    {
        /* Resolve relative path */
        base = g_file_new_for_uri (drop_target);
        child = g_file_get_child (base, filename);
        uri = g_file_get_uri (child);
        g_object_unref (base);
        g_object_unref (child);

        /* Change the uri property */
        gdk_property_change (gdk_drag_context_get_source_window (context),
                             gdk_atom_intern (NAUTILUS_ICON_DND_XDNDDIRECTSAVE_TYPE, FALSE),
                             gdk_atom_intern ("text/plain", FALSE), 8,
                             GDK_PROP_MODE_REPLACE, (const guchar *) uri,
                             strlen (uri));

        drag_info->direct_save_uri = uri;
    }

    g_free (filename);
    g_free (drop_target);
}

/* FIXME bugzilla.gnome.org 47445: Needs to become a shared function */
static void
get_data_on_first_target_we_support (GtkWidget      *widget,
                                     GdkDragContext *context,
                                     guint32         time,
                                     int             x,
                                     int             y)
{
    GtkTargetList *list;
    GdkAtom target;

    if (drop_types_list == NULL)
    {
        drop_types_list = gtk_target_list_new (drop_types,
                                               G_N_ELEMENTS (drop_types) - 1);
        gtk_target_list_add_text_targets (drop_types_list, NAUTILUS_ICON_DND_TEXT);
    }
    if (drop_types_list_root == NULL)
    {
        drop_types_list_root = gtk_target_list_new (drop_types,
                                                    G_N_ELEMENTS (drop_types));
        gtk_target_list_add_text_targets (drop_types_list_root, NAUTILUS_ICON_DND_TEXT);
    }

    list = drop_types_list;

    target = gtk_drag_dest_find_target (widget, context, list);
    if (target != GDK_NONE)
    {
        guint info;
        NautilusDragInfo *drag_info;
        gboolean found;

        drag_info = &(NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info->drag_info);

        found = gtk_target_list_find (list, target, &info);
        g_assert (found);

        /* Don't get_data for destructive ops */
        if ((info == NAUTILUS_ICON_DND_ROOTWINDOW_DROP ||
             info == NAUTILUS_ICON_DND_XDNDDIRECTSAVE) &&
            !drag_info->drop_occurred)
        {
            /* We can't call get_data here, because that would
             *  make the source execute the rootwin action or the direct save */
            drag_info->got_drop_data_type = TRUE;
            drag_info->data_type = info;
        }
        else
        {
            if (info == NAUTILUS_ICON_DND_XDNDDIRECTSAVE)
            {
                set_direct_save_uri (widget, context, drag_info, x, y);
            }
            gtk_drag_get_data (GTK_WIDGET (widget), context,
                               target, time);
        }
    }
}

static void
nautilus_canvas_container_ensure_drag_data (NautilusCanvasContainer *container,
                                            GdkDragContext          *context,
                                            guint32                  time)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = container->details->dnd_info;

    if (!dnd_info->drag_info.got_drop_data_type)
    {
        get_data_on_first_target_we_support (GTK_WIDGET (container), context, time, 0, 0);
    }
}

static void
drag_end_callback (GtkWidget      *widget,
                   GdkDragContext *context,
                   gpointer        data)
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
}

static NautilusCanvasIcon *
nautilus_canvas_container_item_at (NautilusCanvasContainer *container,
                                   int                      x,
                                   int                      y)
{
    GList *p;
    int size;
    EelDRect point;
    EelIRect canvas_point;

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

        eel_canvas_w2c (EEL_CANVAS (container),
                        point.x0,
                        point.y0,
                        &canvas_point.x0,
                        &canvas_point.y0);
        eel_canvas_w2c (EEL_CANVAS (container),
                        point.x1,
                        point.y1,
                        &canvas_point.x1,
                        &canvas_point.y1);
        if (nautilus_canvas_item_hit_test_rectangle (icon->item, canvas_point))
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

/* handle dropped url */
static void
receive_dropped_netscape_url (NautilusCanvasContainer *container,
                              const char              *encoded_url,
                              GdkDragContext          *context,
                              int                      x,
                              int                      y)
{
    char *drop_target;

    if (encoded_url == NULL)
    {
        return;
    }

    drop_target = nautilus_canvas_container_find_drop_target (container, context, x, y, NULL);

    g_signal_emit_by_name (container, "handle-netscape-url",
                           encoded_url,
                           drop_target,
                           gdk_drag_context_get_selected_action (context));

    g_free (drop_target);
}

/* handle dropped uri list */
static void
receive_dropped_uri_list (NautilusCanvasContainer *container,
                          const char              *uri_list,
                          GdkDragContext          *context,
                          int                      x,
                          int                      y)
{
    char *drop_target;

    if (uri_list == NULL)
    {
        return;
    }

    drop_target = nautilus_canvas_container_find_drop_target (container, context, x, y, NULL);

    g_signal_emit_by_name (container, "handle-uri-list",
                           uri_list,
                           drop_target,
                           gdk_drag_context_get_selected_action (context));

    g_free (drop_target);
}

/* handle dropped text */
static void
receive_dropped_text (NautilusCanvasContainer *container,
                      const char              *text,
                      GdkDragContext          *context,
                      int                      x,
                      int                      y)
{
    char *drop_target;

    if (text == NULL)
    {
        return;
    }

    drop_target = nautilus_canvas_container_find_drop_target (container, context, x, y, NULL);

    g_signal_emit_by_name (container, "handle-text",
                           text,
                           drop_target,
                           gdk_drag_context_get_selected_action (context));

    g_free (drop_target);
}

/* handle dropped raw data */
static void
receive_dropped_raw (NautilusCanvasContainer *container,
                     const char              *raw_data,
                     int                      length,
                     const char              *direct_save_uri,
                     GdkDragContext          *context,
                     int                      x,
                     int                      y)
{
    char *drop_target;

    if (raw_data == NULL)
    {
        return;
    }

    drop_target = nautilus_canvas_container_find_drop_target (container, context, x, y, NULL);

    g_signal_emit_by_name (container, "handle-raw",
                           raw_data,
                           length,
                           drop_target,
                           direct_save_uri,
                           gdk_drag_context_get_selected_action (context));

    g_free (drop_target);
}

static int
auto_scroll_timeout_callback (gpointer data)
{
    NautilusCanvasContainer *container;
    GtkWidget *widget;
    float x_scroll_delta, y_scroll_delta;
    GdkRectangle exposed_area;
    GtkAllocation allocation;

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
    dnd_highlight_queue_redraw (widget);

    if (!nautilus_canvas_container_scroll (container, (int) x_scroll_delta, (int) y_scroll_delta))
    {
        /* the scroll value got pinned to a min or max adjustment value,
         * we ended up not scrolling
         */
        return TRUE;
    }

    /* Make sure the dnd highlight frame is redrawn */
    dnd_highlight_queue_redraw (widget);

    /* update cached drag start offsets */
    container->details->dnd_info->drag_info.start_x -= x_scroll_delta;
    container->details->dnd_info->drag_info.start_y -= y_scroll_delta;

    /* Due to a glitch in GtkLayout, whe need to do an explicit draw of the exposed
     * area.
     * Calculate the size of the area we need to draw
     */
    gtk_widget_get_allocation (widget, &allocation);
    exposed_area.x = allocation.x;
    exposed_area.y = allocation.y;
    exposed_area.width = allocation.width;
    exposed_area.height = allocation.height;

    if (x_scroll_delta > 0)
    {
        exposed_area.x = exposed_area.width - x_scroll_delta;
    }
    else if (x_scroll_delta < 0)
    {
        exposed_area.width = -x_scroll_delta;
    }

    if (y_scroll_delta > 0)
    {
        exposed_area.y = exposed_area.height - y_scroll_delta;
    }
    else if (y_scroll_delta < 0)
    {
        exposed_area.height = -y_scroll_delta;
    }

    /* offset it to 0, 0 */
    exposed_area.x -= allocation.x;
    exposed_area.y -= allocation.y;

    gtk_widget_queue_draw_area (widget,
                                exposed_area.x,
                                exposed_area.y,
                                exposed_area.width,
                                exposed_area.height);

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
                                            GdkDragContext          *context,
                                            int                      x,
                                            int                      y,
                                            gboolean                *icon_hit)
{
    NautilusCanvasIcon *drop_target_icon;
    double world_x, world_y;
    NautilusFile *file;
    char *icon_uri;
    char *container_uri;

    if (icon_hit)
    {
        *icon_hit = FALSE;
    }

    if (!container->details->dnd_info->drag_info.got_drop_data_type)
    {
        return NULL;
    }

    canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);

    /* FIXME bugzilla.gnome.org 42485:
     * These "can_accept_items" tests need to be done by
     * the canvas view, not here. This file is not supposed to know
     * that the target is a file.
     */

    /* Find the item we hit with our drop, if any */
    drop_target_icon = nautilus_canvas_container_item_at (container, world_x, world_y);
    if (drop_target_icon != NULL)
    {
        icon_uri = nautilus_canvas_container_get_icon_uri (container, drop_target_icon);
        if (icon_uri != NULL)
        {
            file = nautilus_file_get_by_uri (icon_uri);

            if (!nautilus_drag_can_accept_info (file,
                                                container->details->dnd_info->drag_info.data_type,
                                                container->details->dnd_info->drag_info.selection_list))
            {
                /* the item we dropped our selection on cannot accept the items,
                 * do the same thing as if we just dropped the items on the canvas
                 */
                drop_target_icon = NULL;
            }

            g_free (icon_uri);
            nautilus_file_unref (file);
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
            gboolean can;
            file = nautilus_file_get_by_uri (container_uri);
            can = nautilus_drag_can_accept_info (file,
                                                 container->details->dnd_info->drag_info.data_type,
                                                 container->details->dnd_info->drag_info.selection_list);
            g_object_unref (file);
            if (!can)
            {
                g_free (container_uri);
                container_uri = NULL;
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
                                                 GdkDragContext          *context,
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

    real_action = gdk_drag_context_get_selected_action (context);

    if (real_action == GDK_ACTION_ASK)
    {
        action = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
        real_action = nautilus_drag_drop_action_ask (GTK_WIDGET (container), action);
    }

    if (real_action > 0)
    {
        eel_canvas_window_to_world (EEL_CANVAS (container),
                                    x + gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container))),
                                    y + gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container))),
                                    &world_x, &world_y);

        drop_target = nautilus_canvas_container_find_drop_target (container,
                                                                  context, x, y, &icon_hit);

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
nautilus_canvas_dnd_get_drag_source_data (NautilusCanvasContainer *container,
                                          GdkDragContext          *context)
{
    return container->details->dnd_source_info;
}

static void
nautilus_canvas_container_get_drop_action (NautilusCanvasContainer *container,
                                           GdkDragContext          *context,
                                           int                      x,
                                           int                      y,
                                           int                     *action)
{
    char *drop_target;
    gboolean icon_hit;
    double world_x, world_y;

    icon_hit = FALSE;
    if (!container->details->dnd_info->drag_info.got_drop_data_type)
    {
        /* drag_data_received_callback didn't get called yet */
        return;
    }

    /* find out if we're over an canvas */
    canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);
    *action = 0;

    drop_target = nautilus_canvas_container_find_drop_target (container,
                                                              context, x, y, &icon_hit);
    if (drop_target == NULL)
    {
        return;
    }

    /* case out on the type of object being dragged */
    switch (container->details->dnd_info->drag_info.data_type)
    {
        case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
        {
            if (container->details->dnd_info->drag_info.selection_list != NULL)
            {
                nautilus_drag_default_drop_action_for_icons (context, drop_target,
                                                             container->details->dnd_info->drag_info.selection_list,
                                                             0,
                                                             action);
            }
        }
        break;

        case NAUTILUS_ICON_DND_URI_LIST:
        {
            *action = nautilus_drag_default_drop_action_for_uri_list (context, drop_target);
        }
        break;

        case NAUTILUS_ICON_DND_NETSCAPE_URL:
        {
            *action = nautilus_drag_default_drop_action_for_netscape_url (context);
        }
        break;

        case NAUTILUS_ICON_DND_ROOTWINDOW_DROP:
        {
            *action = gdk_drag_context_get_suggested_action (context);
        }
        break;

        case NAUTILUS_ICON_DND_TEXT:
        case NAUTILUS_ICON_DND_XDNDDIRECTSAVE:
        case NAUTILUS_ICON_DND_RAW:
        {
            *action = GDK_ACTION_COPY;
        }
        break;
    }

    g_free (drop_target);
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
                                        GdkDragContext          *context,
                                        int                      x,
                                        int                      y)
{
    NautilusCanvasIcon *icon;
    NautilusFile *file;
    double world_x, world_y;
    char *uri;

    g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));

    canvas_widget_to_world (EEL_CANVAS (container), x, y, &world_x, &world_y);

    /* Find the item we hit with our drop, if any. */
    icon = nautilus_canvas_container_item_at (container, world_x, world_y);

    /* FIXME bugzilla.gnome.org 42485:
     * These "can_accept_items" tests need to be done by
     * the canvas view, not here. This file is not supposed to know
     * that the target is a file.
     */

    /* Find if target canvas accepts our drop. */
    if (icon != NULL)
    {
        uri = nautilus_canvas_container_get_icon_uri (container, icon);
        file = nautilus_file_get_by_uri (uri);
        g_free (uri);

        if (!nautilus_drag_can_accept_info (file,
                                            container->details->dnd_info->drag_info.data_type,
                                            container->details->dnd_info->drag_info.selection_list))
        {
            icon = NULL;
        }

        nautilus_file_unref (file);
    }

    set_drop_target (container, icon);
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

    dnd_info->drag_info.got_drop_data_type = FALSE;

    if (dnd_info->shadow != NULL)
    {
        eel_canvas_item_destroy (dnd_info->shadow);
        dnd_info->shadow = NULL;
    }

    if (dnd_info->drag_info.selection_data != NULL)
    {
        gtk_selection_data_free (dnd_info->drag_info.selection_data);
        dnd_info->drag_info.selection_data = NULL;
    }

    if (dnd_info->drag_info.direct_save_uri != NULL)
    {
        g_free (dnd_info->drag_info.direct_save_uri);
        dnd_info->drag_info.direct_save_uri = NULL;
    }

    g_free (dnd_info->target_uri);
    dnd_info->target_uri = NULL;

    remove_hover_timer (dnd_info);
}

static void
drag_leave_callback (GtkWidget      *widget,
                     GdkDragContext *context,
                     guint32         time,
                     gpointer        data)
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
drag_begin_callback (GtkWidget      *widget,
                     GdkDragContext *context,
                     gpointer        data)
{
    NautilusCanvasContainer *container;
    NautilusDragInfo *drag_info;
    NautilusWindow *window;
    cairo_surface_t *surface;
    double x1, y1, x2, y2, winx, winy;
    int x_offset, y_offset;
    int start_x, start_y;
    GList *dragged_files;
    double sx, sy;

    container = NAUTILUS_CANVAS_CONTAINER (widget);
    window = NAUTILUS_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (container)));

    start_x = container->details->dnd_info->drag_info.start_x +
              gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));
    start_y = container->details->dnd_info->drag_info.start_y +
              gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container)));

    /* create a pixmap and mask to drag with */
    surface = nautilus_canvas_item_get_drag_surface (container->details->drag_icon->item);

    /* compute the image's offset */
    eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (container->details->drag_icon->item),
                                &x1, &y1, &x2, &y2);
    eel_canvas_world_to_window (EEL_CANVAS (container),
                                x1, y1, &winx, &winy);
    x_offset = start_x - winx;
    y_offset = start_y - winy;

    cairo_surface_get_device_scale (surface, &sx, &sy);
    cairo_surface_set_device_offset (surface,
                                     -x_offset * sx,
                                     -y_offset * sy);
    gtk_drag_set_icon_surface (context, surface);
    cairo_surface_destroy (surface);

    /* cache the data at the beginning since the view may change */
    drag_info = &(container->details->dnd_info->drag_info);
    drag_info->selection_cache = nautilus_drag_create_selection_cache (widget,
                                                                       each_icon_get_data_binder);

    container->details->dnd_source_info->selection_cache = nautilus_drag_create_selection_cache (widget,
                                                                                                 each_icon_get_data_binder);

    dragged_files = nautilus_drag_file_list_from_selection_list (drag_info->selection_cache);
    if (nautilus_file_list_are_all_folders (dragged_files))
    {
        nautilus_window_start_dnd (window, context);
    }
    g_list_free_full (dragged_files, g_object_unref);
}

void
nautilus_canvas_dnd_begin_drag (NautilusCanvasContainer *container,
                                GdkDragAction            actions,
                                int                      button,
                                GdkEventMotion          *event,
                                int                      start_x,
                                int                      start_y)
{
    NautilusCanvasDndInfo *dnd_info;
    NautilusDragInfo *dnd_source_info;

    g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container));
    g_return_if_fail (event != NULL);

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
    gtk_drag_begin_with_coordinates (GTK_WIDGET (container),
                                     dnd_info->drag_info.target_list,
                                     actions,
                                     button,
                                     (GdkEvent *) event,
                                     dnd_info->drag_info.start_x,
                                     dnd_info->drag_info.start_y);
}

static gboolean
drag_highlight_draw (GtkWidget *widget,
                     cairo_t   *cr,
                     gpointer   user_data)
{
    gint width, height;
    GdkWindow *window;
    GtkStyleContext *style;

    window = gtk_widget_get_window (widget);
    width = gdk_window_get_width (window);
    height = gdk_window_get_height (window);

    style = gtk_widget_get_style_context (widget);

    gtk_style_context_save (style);
    gtk_style_context_add_class (style, GTK_STYLE_CLASS_DND);
    gtk_style_context_set_state (style, GTK_STATE_FLAG_FOCUSED);

    gtk_render_frame (style,
                      cr,
                      0, 0, width, height);

    gtk_style_context_restore (style);

    return FALSE;
}

/* Queue a redraw of the dnd highlight rect */
static void
dnd_highlight_queue_redraw (GtkWidget *widget)
{
    NautilusCanvasDndInfo *dnd_info;
    int width, height;
    GtkAllocation allocation;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    if (!dnd_info->highlighted)
    {
        return;
    }

    gtk_widget_get_allocation (widget, &allocation);
    width = allocation.width;
    height = allocation.height;

    /* we don't know how wide the shadow is exactly,
     * so we expose a 10-pixel wide border
     */
    gtk_widget_queue_draw_area (widget,
                                0, 0,
                                width, 10);
    gtk_widget_queue_draw_area (widget,
                                0, 0,
                                10, height);
    gtk_widget_queue_draw_area (widget,
                                0, height - 10,
                                width, 10);
    gtk_widget_queue_draw_area (widget,
                                width - 10, 0,
                                10, height);
}

static void
start_dnd_highlight (GtkWidget *widget)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    if (!dnd_info->highlighted)
    {
        dnd_info->highlighted = TRUE;
        g_signal_connect_after (widget, "draw",
                                G_CALLBACK (drag_highlight_draw),
                                NULL);
        dnd_highlight_queue_redraw (widget);
    }
}

static void
stop_dnd_highlight (GtkWidget *widget)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    if (dnd_info->highlighted)
    {
        g_signal_handlers_disconnect_by_func (widget,
                                              drag_highlight_draw,
                                              NULL);
        dnd_highlight_queue_redraw (widget);
        dnd_info->highlighted = FALSE;
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

    return FALSE;
}

static void
check_hover_timer (NautilusCanvasContainer *container,
                   const char              *uri)
{
    NautilusCanvasDndInfo *dnd_info;
    GtkSettings *settings;
    guint timeout;

    dnd_info = container->details->dnd_info;

    if (g_strcmp0 (uri, dnd_info->target_uri) == 0)
    {
        return;
    }

    remove_hover_timer (dnd_info);

    settings = gtk_widget_get_settings (GTK_WIDGET (container));
    g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);

    g_free (dnd_info->target_uri);
    dnd_info->target_uri = NULL;

    if (uri != NULL)
    {
        dnd_info->target_uri = g_strdup (uri);
        dnd_info->hover_id = g_timeout_add (timeout, hover_timer, container);
    }
}

static gboolean
drag_motion_callback (GtkWidget      *widget,
                      GdkDragContext *context,
                      int             x,
                      int             y,
                      guint32         time)
{
    int action;

    nautilus_canvas_container_ensure_drag_data (NAUTILUS_CANVAS_CONTAINER (widget), context, time);
    nautilus_canvas_container_position_shadow (NAUTILUS_CANVAS_CONTAINER (widget), x, y);
    nautilus_canvas_dnd_update_drop_target (NAUTILUS_CANVAS_CONTAINER (widget), context, x, y);
    set_up_auto_scroll_if_needed (NAUTILUS_CANVAS_CONTAINER (widget));
    /* Find out what the drop actions are based on our drag selection and
     * the drop target.
     */
    action = 0;
    nautilus_canvas_container_get_drop_action (NAUTILUS_CANVAS_CONTAINER (widget), context, x, y,
                                               &action);
    if (action != 0)
    {
        char *uri;
        uri = nautilus_canvas_container_find_drop_target (NAUTILUS_CANVAS_CONTAINER (widget),
                                                          context, x, y, NULL);
        check_hover_timer (NAUTILUS_CANVAS_CONTAINER (widget), uri);
        g_free (uri);
        start_dnd_highlight (widget);
    }
    else
    {
        remove_hover_timer (NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info);
    }

    gdk_drag_status (context, action, time);

    return TRUE;
}

static gboolean
drag_drop_callback (GtkWidget      *widget,
                    GdkDragContext *context,
                    int             x,
                    int             y,
                    guint32         time,
                    gpointer        data)
{
    NautilusCanvasDndInfo *dnd_info;

    dnd_info = NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info;

    /* tell the drag_data_received callback that
     *  the drop occurred and that it can actually
     *  process the actions.
     *  make sure it is going to be called at least once.
     */
    dnd_info->drag_info.drop_occurred = TRUE;

    get_data_on_first_target_we_support (widget, context, time, x, y);

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
                             GdkDragContext   *context,
                             int               x,
                             int               y,
                             GtkSelectionData *data,
                             guint             info,
                             guint32           time,
                             gpointer          user_data)
{
    NautilusDragInfo *drag_info;
    guchar *tmp;
    const guchar *tmp_raw;
    int length;
    gboolean success;

    drag_info = &(NAUTILUS_CANVAS_CONTAINER (widget)->details->dnd_info->drag_info);

    drag_info->got_drop_data_type = TRUE;
    drag_info->data_type = info;

    switch (info)
    {
        case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
        {
            nautilus_canvas_container_dropped_canvas_feedback (widget, data, x, y);
        }
        break;

        case NAUTILUS_ICON_DND_URI_LIST:
        case NAUTILUS_ICON_DND_TEXT:
        case NAUTILUS_ICON_DND_XDNDDIRECTSAVE:
        case NAUTILUS_ICON_DND_RAW:
        {
            /* Save the data so we can do the actual work on drop. */
            if (drag_info->selection_data != NULL)
            {
                gtk_selection_data_free (drag_info->selection_data);
            }
            drag_info->selection_data = gtk_selection_data_copy (data);
        }
        break;

        /* Netscape keeps sending us the data, even though we accept the first drag */
        case NAUTILUS_ICON_DND_NETSCAPE_URL:
        {
            if (drag_info->selection_data != NULL)
            {
                gtk_selection_data_free (drag_info->selection_data);
                drag_info->selection_data = gtk_selection_data_copy (data);
            }
        }
        break;

        case NAUTILUS_ICON_DND_ROOTWINDOW_DROP:
        {
            /* Do nothing, this won't even happen, since we don't want to call get_data twice */
        }
        break;
    }

    /* this is the second use case of this callback.
     * we have to do the actual work for the drop.
     */
    if (drag_info->drop_occurred)
    {
        success = FALSE;
        switch (info)
        {
            case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
            {
                nautilus_canvas_container_receive_dropped_icons
                    (NAUTILUS_CANVAS_CONTAINER (widget),
                    context, x, y);
            }
            break;

            case NAUTILUS_ICON_DND_NETSCAPE_URL:
            {
                receive_dropped_netscape_url
                    (NAUTILUS_CANVAS_CONTAINER (widget),
                    (char *) gtk_selection_data_get_data (data), context, x, y);
                success = TRUE;
            }
            break;

            case NAUTILUS_ICON_DND_URI_LIST:
            {
                receive_dropped_uri_list
                    (NAUTILUS_CANVAS_CONTAINER (widget),
                    (char *) gtk_selection_data_get_data (data), context, x, y);
                success = TRUE;
            }
            break;

            case NAUTILUS_ICON_DND_TEXT:
            {
                tmp = gtk_selection_data_get_text (data);
                receive_dropped_text
                    (NAUTILUS_CANVAS_CONTAINER (widget),
                    (char *) tmp, context, x, y);
                success = TRUE;
                g_free (tmp);
            }
            break;

            case NAUTILUS_ICON_DND_RAW:
            {
                length = gtk_selection_data_get_length (data);
                tmp_raw = gtk_selection_data_get_data (data);
                receive_dropped_raw
                    (NAUTILUS_CANVAS_CONTAINER (widget),
                    (const gchar *) tmp_raw, length, drag_info->direct_save_uri,
                    context, x, y);
                success = TRUE;
            }
            break;

            case NAUTILUS_ICON_DND_ROOTWINDOW_DROP:
            {
                /* Do nothing, everything is done by the sender */
            }
            break;

            case NAUTILUS_ICON_DND_XDNDDIRECTSAVE:
            {
                const guchar *selection_data;
                gint selection_length;
                gint selection_format;

                selection_data = gtk_selection_data_get_data (drag_info->selection_data);
                selection_length = gtk_selection_data_get_length (drag_info->selection_data);
                selection_format = gtk_selection_data_get_format (drag_info->selection_data);

                if (selection_format == 8 &&
                    selection_length == 1 &&
                    selection_data[0] == 'F')
                {
                    gtk_drag_get_data (widget, context,
                                       gdk_atom_intern (NAUTILUS_ICON_DND_RAW_TYPE,
                                                        FALSE),
                                       time);
                    return;
                }
                else if (selection_format == 8 &&
                         selection_length == 1 &&
                         selection_data[0] == 'F' &&
                         drag_info->direct_save_uri != NULL)
                {
                    GFile *location;

                    location = g_file_new_for_uri (drag_info->direct_save_uri);

                    nautilus_file_changes_queue_file_added (location);
                    g_object_unref (location);
                    nautilus_file_changes_consume_changes (TRUE);
                    success = TRUE;
                }
            }     /* NAUTILUS_ICON_DND_XDNDDIRECTSAVE */
            break;
        }
        gtk_drag_finish (context, success, FALSE, time);

        nautilus_canvas_container_free_drag_data (NAUTILUS_CANVAS_CONTAINER (widget));

        set_drop_target (NAUTILUS_CANVAS_CONTAINER (widget), NULL);

        /* reinitialise it for the next dnd */
        drag_info->drop_occurred = FALSE;
    }
}

void
nautilus_canvas_dnd_init (NautilusCanvasContainer *container)
{
    GtkTargetList *targets;
    int n_elements;

    g_return_if_fail (container != NULL);
    g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container));


    container->details->dnd_info = g_new0 (NautilusCanvasDndInfo, 1);
    nautilus_drag_init (&container->details->dnd_info->drag_info,
                        drag_types, G_N_ELEMENTS (drag_types), TRUE);

    /* Set up the widget as a drag destination.
     * (But not a source, as drags starting from this widget will be
     * implemented by dealing with events manually.)
     */
    n_elements = G_N_ELEMENTS (drop_types) - 1;
    gtk_drag_dest_set (GTK_WIDGET (container),
                       0,
                       drop_types, n_elements,
                       GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

    targets = gtk_drag_dest_get_target_list (GTK_WIDGET (container));
    gtk_target_list_add_text_targets (targets, NAUTILUS_ICON_DND_TEXT);


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
