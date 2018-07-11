/*
 * Nautilus
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Dave Camp <dave@ximian.com>
 * XDS support: Benedikt Meurer <benny@xfce.org> (adapted by Amos Brocco <amos.brocco@unifr.ch>)
 */

/* nautilus-tree-view-drag-dest.c: Handles drag and drop for treeviews which
 *                                 contain a hierarchy of files
 */

#include <config.h>

#include "nautilus-tree-view-drag-dest.h"

#include "nautilus-dnd.h"
#include "nautilus-file-changes-queue.h"
#include "nautilus-global-preferences.h"

#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_LIST_VIEW
#include "nautilus-debug.h"

#define AUTO_SCROLL_MARGIN 20
#define HOVER_EXPAND_TIMEOUT 1

struct _NautilusTreeViewDragDestDetails
{
    GtkTreeView *tree_view;

    gboolean drop_occurred;

    gboolean have_drag_data;
    guint drag_type;
    GtkSelectionData *drag_data;
    GList *drag_list;

    guint hover_id;
    guint highlight_id;
    guint scroll_id;
    guint expand_id;

    char *target_uri;

    double drop_x;
    double drop_y;
};

enum
{
    GET_ROOT_URI,
    GET_FILE_FOR_PATH,
    MOVE_COPY_ITEMS,
    HANDLE_URI_LIST,
    HANDLE_TEXT,
    HANDLE_HOVER,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NautilusTreeViewDragDest, nautilus_tree_view_drag_dest,
               G_TYPE_OBJECT);

static const char *drag_types[] =
{
    NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE,
};


static void
gtk_tree_view_vertical_autoscroll (GtkTreeView *tree_view)
{
    GdkRectangle visible_rect;
    GtkAdjustment *vadjustment;
    GdkDisplay *display;
    GdkSeat *seat;
    GdkDevice *pointer;
    GdkSurface *surface;
    int y;
    int offset;
    float value;

    surface = gtk_widget_get_surface (GTK_WIDGET (tree_view));
    vadjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (tree_view));

    display = gtk_widget_get_display (GTK_WIDGET (tree_view));
    seat = gdk_display_get_default_seat (display);
    pointer = gdk_seat_get_pointer (seat);
    gdk_surface_get_device_position (surface, pointer, NULL, &y, NULL);

    y += gtk_adjustment_get_value (vadjustment);

    gtk_tree_view_get_visible_rect (tree_view, &visible_rect);

    offset = y - (visible_rect.y + 2 * AUTO_SCROLL_MARGIN);
    if (offset > 0)
    {
        offset = y - (visible_rect.y + visible_rect.height - 2 * AUTO_SCROLL_MARGIN);
        if (offset < 0)
        {
            return;
        }
    }

    value = CLAMP (gtk_adjustment_get_value (vadjustment) + offset, 0.0,
                   gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment));
    gtk_adjustment_set_value (vadjustment, value);
}

static int
scroll_timeout (gpointer data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (data);

    gtk_tree_view_vertical_autoscroll (tree_view);

    return TRUE;
}

static void
remove_scroll_timeout (NautilusTreeViewDragDest *dest)
{
    if (dest->details->scroll_id)
    {
        g_source_remove (dest->details->scroll_id);
        dest->details->scroll_id = 0;
    }
}

static int
expand_timeout (gpointer data)
{
    GtkTreeView *tree_view;
    GtkTreePath *drop_path;

    tree_view = GTK_TREE_VIEW (data);

    gtk_tree_view_get_drag_dest_row (tree_view, &drop_path, NULL);

    if (drop_path)
    {
        gtk_tree_view_expand_row (tree_view, drop_path, FALSE);
        gtk_tree_path_free (drop_path);
    }

    return FALSE;
}

static void
remove_expand_timer (NautilusTreeViewDragDest *dest)
{
    if (dest->details->expand_id)
    {
        g_source_remove (dest->details->expand_id);
        dest->details->expand_id = 0;
    }
}

static void
set_drag_dest_row (NautilusTreeViewDragDest *dest,
                   GtkTreePath              *path)
{
    if (path)
    {
        gtk_tree_view_set_drag_dest_row (dest->details->tree_view,
                                         path,
                                         GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
    }
    else
    {
        gtk_tree_view_set_drag_dest_row (dest->details->tree_view, NULL, 0);
    }
}

static void
clear_drag_dest_row (NautilusTreeViewDragDest *dest)
{
    gtk_tree_view_set_drag_dest_row (dest->details->tree_view, NULL, 0);
}

static gboolean
get_drag_data (NautilusTreeViewDragDest *dest,
               GdkDrop                  *drop)
{
    GdkAtom target;

    target = gtk_drag_dest_find_target (GTK_WIDGET (dest->details->tree_view),
                                        drop,
                                        NULL);

    if (target == NULL)
    {
        return FALSE;
    }

    gtk_drag_get_data (GTK_WIDGET (dest->details->tree_view), drop, target);

    return TRUE;
}

static void
remove_hover_timer (NautilusTreeViewDragDest *dest)
{
    if (dest->details->hover_id != 0)
    {
        g_source_remove (dest->details->hover_id);
        dest->details->hover_id = 0;
    }
}

static void
free_drag_data (NautilusTreeViewDragDest *dest)
{
    dest->details->have_drag_data = FALSE;

    if (dest->details->drag_data)
    {
        gtk_selection_data_free (dest->details->drag_data);
        dest->details->drag_data = NULL;
    }

    if (dest->details->drag_list)
    {
        nautilus_drag_destroy_selection_list (dest->details->drag_list);
        dest->details->drag_list = NULL;
    }

    g_free (dest->details->target_uri);
    dest->details->target_uri = NULL;

    remove_hover_timer (dest);
    remove_expand_timer (dest);
}

static gboolean
hover_timer (gpointer user_data)
{
    NautilusTreeViewDragDest *dest = user_data;

    dest->details->hover_id = 0;

    g_signal_emit (dest, signals[HANDLE_HOVER], 0, dest->details->target_uri);

    return FALSE;
}

static void
check_hover_timer (NautilusTreeViewDragDest *dest,
                   const char               *uri)
{
    GtkSettings *settings;
    guint timeout;

    if (g_strcmp0 (uri, dest->details->target_uri) == 0)
    {
        return;
    }
    remove_hover_timer (dest);

    settings = gtk_widget_get_settings (GTK_WIDGET (dest->details->tree_view));
    g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);

    g_free (dest->details->target_uri);
    dest->details->target_uri = NULL;

    if (uri != NULL)
    {
        dest->details->target_uri = g_strdup (uri);
        dest->details->hover_id = g_timeout_add (timeout, hover_timer, dest);
    }
}

static void
check_expand_timer (NautilusTreeViewDragDest *dest,
                    GtkTreePath              *drop_path,
                    GtkTreePath              *old_drop_path)
{
    GtkTreeModel *model;
    GtkTreeIter drop_iter;

    model = gtk_tree_view_get_model (dest->details->tree_view);

    if (drop_path == NULL ||
        (old_drop_path != NULL && gtk_tree_path_compare (old_drop_path, drop_path) != 0))
    {
        remove_expand_timer (dest);
    }

    if (dest->details->expand_id == 0 &&
        drop_path != NULL)
    {
        gtk_tree_model_get_iter (model, &drop_iter, drop_path);
        if (gtk_tree_model_iter_has_child (model, &drop_iter))
        {
            dest->details->expand_id =
                g_timeout_add_seconds (HOVER_EXPAND_TIMEOUT,
                                       expand_timeout,
                                       dest->details->tree_view);
        }
    }
}

static char *
get_root_uri (NautilusTreeViewDragDest *dest)
{
    char *uri;

    g_signal_emit (dest, signals[GET_ROOT_URI], 0, &uri);

    return uri;
}

static NautilusFile *
file_for_path (NautilusTreeViewDragDest *dest,
               GtkTreePath              *path)
{
    NautilusFile *file;
    char *uri;

    if (path)
    {
        g_signal_emit (dest, signals[GET_FILE_FOR_PATH], 0, path, &file);
    }
    else
    {
        uri = get_root_uri (dest);

        file = NULL;
        if (uri != NULL)
        {
            file = nautilus_file_get_by_uri (uri);
        }

        g_free (uri);
    }

    return file;
}

static char *
get_drop_target_uri_for_path (NautilusTreeViewDragDest *dest,
                              GtkTreePath              *path,
                              GdkDrop                  *drop)
{
    NautilusFile *file;
    char *target = NULL;
    gboolean can;

    file = file_for_path (dest, path);
    if (file == NULL)
    {
        return NULL;
    }
    can = nautilus_drag_can_accept_data (file,
                                         drop,
                                         dest->details->drag_list);
    if (can)
    {
        target = nautilus_file_get_uri (file);
    }
    nautilus_file_unref (file);

    return target;
}

static void
check_hover_expand_timer (NautilusTreeViewDragDest *dest,
                          GtkTreePath              *path,
                          GtkTreePath              *drop_path,
                          GtkTreePath              *old_drop_path,
                          GdkDrop                  *drop)
{
    gboolean use_tree = g_settings_get_boolean (nautilus_list_view_preferences,
                                                NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);

    if (use_tree)
    {
        check_expand_timer (dest, drop_path, old_drop_path);
    }
    else
    {
        char *uri;
        uri = get_drop_target_uri_for_path (dest, path, drop);
        check_hover_timer (dest, uri);
        g_free (uri);
    }
}

static GtkTreePath *
get_drop_path (NautilusTreeViewDragDest *dest,
               GtkTreePath              *path,
               GdkDrop                  *drop)
{
    NautilusFile *file;
    GtkTreePath *ret;

    if (!path || !dest->details->have_drag_data)
    {
        return NULL;
    }

    ret = gtk_tree_path_copy (path);
    file = file_for_path (dest, ret);

    /* Go up the tree until we find a file that can accept a drop */
    while (file == NULL /* dummy row */ ||
           !nautilus_drag_can_accept_data (file,
                                           drop,
                                           dest->details->drag_list))
    {
        if (gtk_tree_path_get_depth (ret) == 1)
        {
            gtk_tree_path_free (ret);
            ret = NULL;
            break;
        }
        else
        {
            gtk_tree_path_up (ret);

            nautilus_file_unref (file);
            file = file_for_path (dest, ret);
        }
    }
    nautilus_file_unref (file);

    return ret;
}

static GdkDragAction
get_drop_actions (NautilusTreeViewDragDest *dest,
                  GdkDrop                  *drop,
                  GtkTreePath              *path)
{
    GdkContentFormats *formats;
    g_autofree char *drop_target = NULL;
    GdkDragAction actions = 0;

    formats = gdk_drop_get_formats (drop);
    if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE) &&
        dest->details->drag_list == NULL)
    {
        return 0;
    }
    drop_target = get_drop_target_uri_for_path (dest, path, drop);
    if (drop_target == NULL)
    {
        return 0;
    }

    if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
    {
        actions = nautilus_get_drop_actions_for_icons (drop_target,
                                                       dest->details->drag_list);
    }
    else if (nautilus_content_formats_include_text (formats) ||
             nautilus_content_formats_include_uri (formats))
    {
        actions = GDK_ACTION_COPY;
    }

    return actions;
}

static gboolean
drag_motion_callback (GtkWidget *widget,
                      GdkDrop   *drop,
                      int        x,
                      int        y,
                      gpointer   data)
{
    NautilusTreeViewDragDest *dest;
    GdkDragAction actions;
    GtkTreePath *path;
    GtkTreePath *drop_path, *old_drop_path;
    GtkTreeViewDropPosition pos;
    GdkSurface *surface;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);
    actions = 0;

    gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget), x, y, &path, &pos);

    if (pos == GTK_TREE_VIEW_DROP_BEFORE || pos == GTK_TREE_VIEW_DROP_AFTER)
    {
        gtk_tree_path_free (path);
        path = NULL;
    }

    dest->details->drop_x = x;
    dest->details->drop_y = y;

    if (!dest->details->have_drag_data && !get_drag_data (dest, drop))
    {
        return FALSE;
    }

    drop_path = get_drop_path (dest, path, drop);

    surface = gtk_widget_get_surface (widget);
    if (surface != NULL)
    {
        int surface_x, surface_y;
        gdk_surface_get_position (surface, &surface_x, &surface_y);
        if (surface_y <= y)
        {
            /* ignore drags on the header */
            actions = get_drop_actions (dest, drop, drop_path);
        }
    }

    gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (widget), &old_drop_path,
                                     NULL);

    if (actions != 0)
    {
        set_drag_dest_row (dest, drop_path);
        check_hover_expand_timer (dest, path, drop_path, old_drop_path, drop);
    }
    else
    {
        clear_drag_dest_row (dest);
        remove_hover_timer (dest);
        remove_expand_timer (dest);
    }

    if (path)
    {
        gtk_tree_path_free (path);
    }

    if (drop_path)
    {
        gtk_tree_path_free (drop_path);
    }

    if (old_drop_path)
    {
        gtk_tree_path_free (old_drop_path);
    }

    if (dest->details->scroll_id == 0)
    {
        dest->details->scroll_id =
            g_timeout_add (150,
                           scroll_timeout,
                           dest->details->tree_view);
    }

    gdk_drop_status (drop, actions);

    return TRUE;
}

static void
drag_leave_callback (GtkWidget *widget,
                     GdkDrop   *drop,
                     gpointer   data)
{
    NautilusTreeViewDragDest *dest;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

    clear_drag_dest_row (dest);

    free_drag_data (dest);

    remove_scroll_timeout (dest);
}

static char *
get_drop_target_uri_at_pos (NautilusTreeViewDragDest *dest,
                            GdkDrop                  *drop,
                            int                       x,
                            int                       y)
{
    char *drop_target = NULL;
    GtkTreePath *path;
    GtkTreePath *drop_path;
    GtkTreeViewDropPosition pos;

    gtk_tree_view_get_dest_row_at_pos (dest->details->tree_view, x, y,
                                       &path, &pos);
    if (pos == GTK_TREE_VIEW_DROP_BEFORE ||
        pos == GTK_TREE_VIEW_DROP_AFTER)
    {
        gtk_tree_path_free (path);
        path = NULL;
    }

    drop_path = get_drop_path (dest, path, drop);

    drop_target = get_drop_target_uri_for_path (dest, drop_path, drop);

    if (path != NULL)
    {
        gtk_tree_path_free (path);
    }

    if (drop_path != NULL)
    {
        gtk_tree_path_free (drop_path);
    }

    return drop_target;
}

static void
receive_uris (NautilusTreeViewDragDest *dest,
              GdkDrop                  *drop,
              GList                    *source_uris,
              int                       x,
              int                       y)
{
    GdkContentFormats *formats;
    char *drop_target;
    GdkDragAction actions;

    formats = gdk_drop_get_formats (drop);
    drop_target = get_drop_target_uri_at_pos (dest, drop, x, y);
    g_assert (drop_target != NULL);

    actions = gdk_drop_get_actions (drop);
    if (!gdk_drag_action_is_unique (actions))
    {
        actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
        actions = nautilus_drag_drop_action_ask (GTK_WIDGET (dest->details->tree_view), actions);
    }

    /* We only want to copy external uris */
    if (nautilus_content_formats_include_uri (formats) &&
        !gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
    {
        actions = GDK_ACTION_COPY;
    }

    if (actions != 0)
    {
        if (!nautilus_drag_uris_local (drop_target, source_uris) || actions != GDK_ACTION_MOVE)
        {
            g_signal_emit (dest, signals[MOVE_COPY_ITEMS], 0,
                           source_uris,
                           drop_target,
                           actions,
                           x, y);
        }
    }

    g_free (drop_target);
}

static void
receive_dropped_icons (NautilusTreeViewDragDest *dest,
                       GdkDrop                  *drop,
                       int                       x,
                       int                       y)
{
    GList *source_uris;
    GList *l;

    /* FIXME: ignore local only moves */

    if (!dest->details->drag_list)
    {
        return;
    }

    source_uris = NULL;
    for (l = dest->details->drag_list; l != NULL; l = l->next)
    {
        source_uris = g_list_prepend (source_uris,
                                      ((NautilusDragSelectionItem *) l->data)->uri);
    }

    source_uris = g_list_reverse (source_uris);

    receive_uris (dest, drop, source_uris, x, y);

    g_list_free (source_uris);
}

static void
receive_dropped_uri_list (NautilusTreeViewDragDest *dest,
                          GdkDrop                  *drop,
                          int                       x,
                          int                       y)
{
    char *drop_target;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target = get_drop_target_uri_at_pos (dest, drop, x, y);
    g_assert (drop_target != NULL);

    g_signal_emit (dest, signals[HANDLE_URI_LIST], 0,
                   (char *) gtk_selection_data_get_data (dest->details->drag_data),
                   drop_target,
                   gdk_drop_get_actions (drop),
                   x, y);

    g_free (drop_target);
}

static void
receive_dropped_text (NautilusTreeViewDragDest *dest,
                      GdkDrop                  *drop,
                      int                       x,
                      int                       y)
{
    char *drop_target;
    guchar *text;

    if (!dest->details->drag_data)
    {
        return;
    }

    drop_target = get_drop_target_uri_at_pos (dest, drop, x, y);
    g_assert (drop_target != NULL);

    text = gtk_selection_data_get_text (dest->details->drag_data);
    g_signal_emit (dest, signals[HANDLE_TEXT], 0,
                   (char *) text, drop_target,
                   gdk_drop_get_actions (drop),
                   x, y);

    g_free (text);
    g_free (drop_target);
}

static gboolean
drag_data_received_callback (GtkWidget        *widget,
                             GdkDrop          *drop,
                             GtkSelectionData *selection_data,
                             gpointer          data)
{
    NautilusTreeViewDragDest *dest;
    gboolean finished;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

    if (!dest->details->have_drag_data)
    {
        GdkContentFormats *formats;

        formats = gdk_drop_get_formats (drop);

        dest->details->have_drag_data = TRUE;
        dest->details->drag_data = gtk_selection_data_copy (selection_data);

        if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
        {
            dest->details->drag_list = nautilus_drag_build_selection_list (selection_data);
        }
    }

    if (dest->details->drop_occurred)
    {
        GdkContentFormats *formats;

        formats = gdk_drop_get_formats (drop);
        finished = TRUE;

        if (gdk_content_formats_contain_mime_type (formats, NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE))
        {
            receive_dropped_icons (dest, drop, dest->details->drop_x, dest->details->drop_y);
        }
        else if (nautilus_content_formats_include_uri (formats))
        {
            receive_dropped_uri_list (dest, drop, dest->details->drop_x, dest->details->drop_y);
        }
        else if (nautilus_content_formats_include_text (formats))
        {
            receive_dropped_text (dest, drop, dest->details->drop_x, dest->details->drop_y);
        }

        if (finished)
        {
            dest->details->drop_occurred = FALSE;
            free_drag_data (dest);
            gdk_drop_finish (drop, gdk_drop_get_actions (drop));
        }
    }

    /* appease GtkTreeView by preventing its drag_data_receive
     * from being called */
    g_signal_stop_emission_by_name (dest->details->tree_view,
                                    "drag-data-received");

    return TRUE;
}

static gboolean
drag_drop_callback (GtkWidget *widget,
                    GdkDrop   *drop,
                    int        x,
                    int        y,
                    gpointer   data)
{
    NautilusTreeViewDragDest *dest;
    GdkAtom target;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (data);

    target = gtk_drag_dest_find_target (GTK_WIDGET (dest->details->tree_view), drop, NULL);
    if (target == NULL)
    {
        return FALSE;
    }

    dest->details->drop_occurred = TRUE;
    dest->details->drop_x = x;
    dest->details->drop_y = y;

    get_drag_data (dest, drop);
    remove_scroll_timeout (dest);
    clear_drag_dest_row (dest);

    return TRUE;
}

static void
tree_view_weak_notify (gpointer  user_data,
                       GObject  *object)
{
    NautilusTreeViewDragDest *dest;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (user_data);

    remove_scroll_timeout (dest);

    dest->details->tree_view = NULL;
}

static void
nautilus_tree_view_drag_dest_dispose (GObject *object)
{
    NautilusTreeViewDragDest *dest;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (object);

    if (dest->details->tree_view)
    {
        g_object_weak_unref (G_OBJECT (dest->details->tree_view),
                             tree_view_weak_notify,
                             dest);
    }

    remove_scroll_timeout (dest);

    G_OBJECT_CLASS (nautilus_tree_view_drag_dest_parent_class)->dispose (object);
}

static void
nautilus_tree_view_drag_dest_finalize (GObject *object)
{
    NautilusTreeViewDragDest *dest;

    dest = NAUTILUS_TREE_VIEW_DRAG_DEST (object);
    free_drag_data (dest);

    G_OBJECT_CLASS (nautilus_tree_view_drag_dest_parent_class)->finalize (object);
}

static void
nautilus_tree_view_drag_dest_init (NautilusTreeViewDragDest *dest)
{
    dest->details = G_TYPE_INSTANCE_GET_PRIVATE (dest, NAUTILUS_TYPE_TREE_VIEW_DRAG_DEST,
                                                 NautilusTreeViewDragDestDetails);
}

static void
nautilus_tree_view_drag_dest_class_init (NautilusTreeViewDragDestClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);

    gobject_class->dispose = nautilus_tree_view_drag_dest_dispose;
    gobject_class->finalize = nautilus_tree_view_drag_dest_finalize;

    g_type_class_add_private (class, sizeof (NautilusTreeViewDragDestDetails));

    signals[GET_ROOT_URI] =
        g_signal_new ("get-root-uri",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
                                       get_root_uri),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_STRING, 0);
    signals[GET_FILE_FOR_PATH] =
        g_signal_new ("get-file-for-path",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
                                       get_file_for_path),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      NAUTILUS_TYPE_FILE, 1,
                      GTK_TYPE_TREE_PATH);
    signals[MOVE_COPY_ITEMS] =
        g_signal_new ("move-copy-items",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
                                       move_copy_items),
                      NULL, NULL,

                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 3,
                      G_TYPE_POINTER,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION);
    signals[HANDLE_URI_LIST] =
        g_signal_new ("handle-uri-list",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
                                       handle_uri_list),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 3,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION);
    signals[HANDLE_TEXT] =
        g_signal_new ("handle-text",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
                                       handle_text),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 3,
                      G_TYPE_STRING,
                      G_TYPE_STRING,
                      GDK_TYPE_DRAG_ACTION);
    signals[HANDLE_HOVER] =
        g_signal_new ("handle-hover",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusTreeViewDragDestClass,
                                       handle_hover),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 1,
                      G_TYPE_STRING);
}



NautilusTreeViewDragDest *
nautilus_tree_view_drag_dest_new (GtkTreeView *tree_view)
{
    g_autoptr (GdkContentFormats) formats = NULL;
    NautilusTreeViewDragDest *dest;

    formats = gdk_content_formats_new (drag_types, G_N_ELEMENTS (drag_types));
    formats = gtk_content_formats_add_text_targets (formats);
    formats = gtk_content_formats_add_uri_targets (formats);
    dest = g_object_new (NAUTILUS_TYPE_TREE_VIEW_DRAG_DEST, NULL);

    dest->details->tree_view = tree_view;
    g_object_weak_ref (G_OBJECT (dest->details->tree_view),
                       tree_view_weak_notify, dest);

    gtk_drag_dest_set (GTK_WIDGET (tree_view),
                       0, formats,
                       GDK_ACTION_ALL);

    g_signal_connect_object (tree_view,
                             "drag-motion",
                             G_CALLBACK (drag_motion_callback),
                             dest, 0);
    g_signal_connect_object (tree_view,
                             "drag-leave",
                             G_CALLBACK (drag_leave_callback),
                             dest, 0);
    g_signal_connect_object (tree_view,
                             "drag-drop",
                             G_CALLBACK (drag_drop_callback),
                             dest, 0);
    g_signal_connect_object (tree_view,
                             "drag-data-received",
                             G_CALLBACK (drag_data_received_callback),
                             dest, 0);

    return dest;
}
