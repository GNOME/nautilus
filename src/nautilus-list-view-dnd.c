/* nautilus-list-view-dnd.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "nautilus-list-view-dnd.h"
#include "nautilus-list-view-private.h"

static GdkContentFormats *source_targets;

static void
drag_info_data_free (NautilusListView *list_view);

static void
drag_data_get_callback (GtkWidget        *widget,
                        GdkDrag          *context,
                        GtkSelectionData *selection_data,
                        gpointer          user_data)
{
    GtkTreeView *tree_view;
    GtkTreeModel *model;
    NautilusListView *list_view;

    tree_view = GTK_TREE_VIEW (widget);
    list_view = NAUTILUS_LIST_VIEW (user_data);

    model = gtk_tree_view_get_model (tree_view);

    if (model == NULL)
    {
        return;
    }

    if (list_view->details->drag_source_info == NULL ||
        list_view->details->drag_source_info->selection_cache == NULL)
    {
        return;
    }

    nautilus_drag_drag_data_get_from_cache (list_view->details->drag_source_info->selection_cache,
                                            selection_data);
}

static GdkTexture *
get_drag_texture (NautilusListView *view)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    GdkTexture *ret;
    GdkRectangle cell_area;

    ret = NULL;

    if (gtk_tree_view_get_path_at_pos (view->details->tree_view,
                                       view->details->drag_x,
                                       view->details->drag_y,
                                       &path, NULL, NULL, NULL))
    {
        model = gtk_tree_view_get_model (view->details->tree_view);
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter,
                            nautilus_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
                            &ret,
                            -1);
    }

    gtk_tree_view_get_cell_area (view->details->tree_view,
                                 path,
                                 view->details->file_name_column,
                                 &cell_area);

    gtk_tree_path_free (path);

    return ret;
}

/* iteration glue struct */
typedef struct
{
    NautilusListView *view;
    NautilusDragEachSelectedItemDataGet iteratee;
    gpointer iteratee_data;
} ListGetDataBinderContext;

static void
item_get_data_binder (GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      data)
{
    ListGetDataBinderContext *context = data;
    NautilusFile *file;
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GdkRectangle cell_area;
    int drag_begin_y = 0;
    char *uri;

    treeview = nautilus_list_model_get_drag_view (context->view->details->model,
                                                  NULL,
                                                  &drag_begin_y);
    column = gtk_tree_view_get_column (treeview, 0);

    file = nautilus_list_model_file_for_path (NAUTILUS_LIST_MODEL (model), path);
    if (file == NULL)
    {
        return;
    }

    gtk_tree_view_get_cell_area (treeview,
                                 path,
                                 column,
                                 &cell_area);

    uri = nautilus_file_get_activation_uri (file);

    nautilus_file_unref (file);

    /* pass the uri, mouse-relative x/y and icon width/height */
    context->iteratee (uri,
                       0,
                       cell_area.y - drag_begin_y,
                       cell_area.width,
                       cell_area.height,
                       context->iteratee_data);

    g_free (uri);
}

static void
each_item_get_data_binder (NautilusDragEachSelectedItemDataGet iteratee,
                           gpointer                            iterator_context,
                           gpointer                            data)
{
    NautilusListView *view = NAUTILUS_LIST_VIEW (iterator_context);
    ListGetDataBinderContext context;
    GtkTreeSelection *selection;

    context.view = view;
    context.iteratee = iteratee;
    context.iteratee_data = data;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_view));
    gtk_tree_selection_selected_foreach (selection, item_get_data_binder, &context);
}

static void
drag_begin_callback (GtkWidget        *widget,
                     GdkDrag          *context,
                     NautilusListView *view)
{
    g_autoptr (GdkTexture) texture = NULL;
    NautilusWindow *window;
    GList *dragged_files;

    window = nautilus_files_view_get_window (NAUTILUS_FILES_VIEW (view));
    texture = get_drag_texture (view);
    if (texture != NULL)
    {
        gtk_drag_set_icon_paintable (context, GDK_PAINTABLE (texture), 0, 0);
    }
    else
    {
        gtk_drag_set_icon_default (context);
    }

    view->details->drag_button = 0;
    view->details->drag_started = TRUE;

    view->details->drag_source_info->selection_cache = nautilus_drag_create_selection_cache (view,
                                                                                             each_item_get_data_binder);

    dragged_files = nautilus_drag_file_list_from_selection_list (view->details->drag_source_info->selection_cache);
    if (nautilus_file_list_are_all_folders (dragged_files))
    {
        nautilus_window_start_dnd (window, context);
    }
    g_list_free_full (dragged_files, g_object_unref);
}

static void
drag_end_callback (GtkWidget        *widget,
                   GdkDrag          *context,
                   NautilusListView *list_view)
{
    NautilusWindow *window;

    window = nautilus_files_view_get_window (NAUTILUS_FILES_VIEW (list_view));

    nautilus_window_end_dnd (window, context);

    drag_info_data_free (list_view);
}

static void
drag_info_data_free (NautilusListView *list_view)
{
    nautilus_drag_destroy_selection_list (list_view->details->drag_source_info->selection_cache);
    list_view->details->drag_source_info->selection_cache = NULL;

    g_free (list_view->details->drag_source_info);
    list_view->details->drag_source_info = NULL;

    g_signal_handlers_disconnect_by_func (list_view->details->tree_view, drag_begin_callback, list_view);
    g_signal_handlers_disconnect_by_func (list_view->details->tree_view, drag_data_get_callback, list_view);
    g_signal_handlers_disconnect_by_func (list_view->details->tree_view, drag_end_callback, list_view);
}

NautilusDragInfo *
nautilus_list_view_dnd_get_drag_source_data (NautilusListView *list_view)
{
    GtkTreeView *tree_view;
    GtkTreeModel *model;

    tree_view = GTK_TREE_VIEW (list_view->details->tree_view);

    model = gtk_tree_view_get_model (tree_view);

    if (model == NULL)
    {
        return NULL;
    }

    if (list_view->details->drag_source_info == NULL ||
        list_view->details->drag_source_info->selection_cache == NULL)
    {
        return NULL;
    }

    return list_view->details->drag_source_info;
}

void
nautilus_list_view_dnd_init (NautilusListView *list_view)
{
    if (list_view->details->drag_source_info != NULL)
    {
        return;
    }

    list_view->details->drag_source_info = g_new0 (NautilusDragInfo, 1);

    g_signal_connect_object (list_view->details->tree_view, "drag-begin",
                             G_CALLBACK (drag_begin_callback), list_view, 0);
    g_signal_connect_object (list_view->details->tree_view, "drag-end",
                             G_CALLBACK (drag_end_callback), list_view, 0);
    g_signal_connect_object (list_view->details->tree_view, "drag-data-get",
                             G_CALLBACK (drag_data_get_callback), list_view, 0);
}

void
nautilus_list_view_dnd_drag_begin (NautilusListView *list_view,
                                   gdouble           offset_x,
                                   gdouble           offset_y,
                                   const GdkEvent   *event)
{
    if (list_view->details->drag_button == 0)
    {
        return;
    }

    if (source_targets == NULL)
    {
        source_targets = nautilus_list_model_get_drag_targets ();
    }

    if (gtk_drag_check_threshold (GTK_WIDGET (list_view->details->tree_view),
                                  list_view->details->drag_x,
                                  list_view->details->drag_y,
                                  list_view->details->drag_x + offset_x,
                                  list_view->details->drag_y + offset_y))
    {
        GdkDragAction actions;

        actions = GDK_ACTION_ALL | GDK_ACTION_ASK;

        list_view->details->drag_source_info->source_actions = actions;
        gtk_drag_begin (GTK_WIDGET (list_view->details->tree_view),
                                    NULL,
                                    source_targets,
                                    actions,
                                    list_view->details->drag_x,
                                    list_view->details->drag_y);
    }
}
