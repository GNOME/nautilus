/* fm-list-view.c - implementation of list view of directory.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *  Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>
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
 *  Authors: John Sullivan <sullivan@eazel.com>
 *           Anders Carlsson <andersca@gnu.org>
 *           David Emory Watson <dwatson@cs.ucr.edu>
 */

#include "nautilus-list-view.h"
#include "nautilus-list-view-private.h"

#include <eel/eel-vfs-extensions.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_LIST_VIEW
#include "nautilus-debug.h"

#include "nautilus-clipboard.h"
#include "nautilus-column-chooser.h"
#include "nautilus-column-utilities.h"
#include "nautilus-dnd.h"
#include "nautilus-enums.h"
#include "nautilus-error-reporting.h"
#include "nautilus-file-utilities.h"
#include "nautilus-files-view-dnd.h"
#include "nautilus-global-preferences.h"
#include "nautilus-list-model.h"
#include "nautilus-list-view-dnd.h"
#include "nautilus-metadata.h"
#include "nautilus-search-directory.h"
#include "nautilus-tag-manager.h"
#include "nautilus-toolbar.h"
#include "nautilus-tree-view-drag-dest.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-view.h"
#include "nautilus-tracker-utilities.h"
#include "nautilus-gtk4-helpers.h"

struct SelectionForeachData
{
    GList *list;
    GtkTreeSelection *selection;
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT    28

/* The star icon itself is 16px, which leaves an empty 16px gutter on each side,
 * which is necessary to avoid the overlay scrollbar.
 */
#define STAR_COLUMN_WIDTH 48

/* We wait two seconds after row is collapsed to unload the subdirectory */
#define COLLAPSE_TO_UNLOAD_DELAY 2

/* According to Pango docs, alpha is a guint16 value between 0 and 65535. */
#define ALPHA_55_PERCENT ((guint16) (0.55 * 0xffff))

static GdkCursor *hand_cursor = NULL;

static GList *nautilus_list_view_get_selection (NautilusFilesView *view);
static GList *nautilus_list_view_get_selection_for_file_transfer (NautilusFilesView *view);
static void   nautilus_list_view_set_zoom_level (NautilusListView     *view,
                                                 NautilusListZoomLevel new_level);
static void   nautilus_list_view_scroll_to_file (NautilusListView *view,
                                                 NautilusFile     *file);
static void   nautilus_list_view_sort_directories_first_changed (NautilusFilesView *view);

static void   apply_columns_settings (NautilusListView *list_view,
                                      char            **column_order,
                                      char            **visible_columns);
static char **get_visible_columns (NautilusListView *list_view);
static char **get_default_visible_columns (NautilusListView *list_view);
static char **get_column_order (NautilusListView *list_view);
static char **get_default_column_order (NautilusListView *list_view);
static void on_clipboard_owner_changed (GtkClipboard *clipboard,
                                        GdkEvent     *event,
                                        gpointer      user_data);
static void popup_column_header_menu (NautilusListView *list_view,
                                      gdouble           x,
                                      gdouble           y);

G_DEFINE_TYPE (NautilusListView, nautilus_list_view, NAUTILUS_TYPE_FILES_VIEW);

static const char *default_search_visible_columns[] =
{
    "name", "size", "where", NULL
};

static const char *default_search_columns_order[] =
{
    "name", "size", "where", NULL
};

static const char *default_recent_visible_columns[] =
{
    "name", "where", "recency", NULL
};

static const char *default_recent_columns_order[] =
{
    "name", "where", "recency", NULL
};

static const char *default_trash_visible_columns[] =
{
    "name", "size", "trash_orig_path", "trashed_on", NULL
};

static const char *default_trash_columns_order[] =
{
    "name", "size", "trash_orig_path", "trashed_on", NULL
};

static const gchar *
get_default_sort_order (NautilusFile *file,
                        gboolean     *reversed)
{
    NautilusFileSortType sort_type;

    /* This array makes the #NautilusFileSortType values correspond to the
     * respective column attribute.
     */
    const char *attributes[] =
    {
        "name",
        "size",
        "type",
        "date_modified",
        "date_accessed",
        "date_created",
        "starred",
        "trashed_on",
        "search_relevance",
        "recency",
        NULL
    };

    sort_type = nautilus_file_get_default_sort_type (file, reversed);

    return attributes[sort_type];
}

static void
list_selection_changed_callback (GtkTreeSelection *selection,
                                 gpointer          user_data)
{
    NautilusFilesView *view;

    view = NAUTILUS_FILES_VIEW (user_data);

    nautilus_files_view_notify_selection_changed (view);
}

static void
activate_selected_items (NautilusListView *view)
{
    GList *file_list;

    file_list = nautilus_list_view_get_selection (NAUTILUS_FILES_VIEW (view));
    if (file_list != NULL)
    {
        nautilus_files_view_activate_files (NAUTILUS_FILES_VIEW (view),
                                            file_list,
                                            0, TRUE);
        nautilus_file_list_free (file_list);
    }
}

static void
activate_selected_items_alternate (NautilusListView *view,
                                   NautilusFile     *file,
                                   gboolean          open_in_tab)
{
    GList *file_list;
    NautilusOpenFlags flags;

    flags = 0;

    if (open_in_tab)
    {
        flags |= NAUTILUS_OPEN_FLAG_NEW_TAB;
        flags |= NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;
    }
    else
    {
        flags |= NAUTILUS_OPEN_FLAG_NEW_WINDOW;
    }

    if (file != NULL)
    {
        nautilus_file_ref (file);
        file_list = g_list_prepend (NULL, file);
    }
    else
    {
        file_list = nautilus_list_view_get_selection (NAUTILUS_FILES_VIEW (view));
    }
    nautilus_files_view_activate_files (NAUTILUS_FILES_VIEW (view),
                                        file_list,
                                        flags,
                                        TRUE);
    nautilus_file_list_free (file_list);
}

static gboolean
button_event_modifies_selection (const GdkEvent *event)
{
    GdkModifierType state;

    gdk_event_get_state (event, &state);

    return (state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static int
get_click_policy (void)
{
    return g_settings_get_enum (nautilus_preferences,
                                NAUTILUS_PREFERENCES_CLICK_POLICY);
}

static void
on_event_controller_motion_motion (GtkEventControllerMotion *controller,
                                   double                    x,
                                   double                    y,
                                   gpointer                  user_data)
{
    NautilusListView *view;
    GtkWidget *widget;
    GtkTreePath *old_hover_path;
    int x_in_bin;
    int y_in_bin;

    if (get_click_policy () != NAUTILUS_CLICK_POLICY_SINGLE)
    {
        return;
    }

    view = user_data;
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    old_hover_path = view->details->hover_path;

    gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (widget),
                                                       x, y,
                                                       &x_in_bin, &y_in_bin);
    gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                   x_in_bin, y_in_bin,
                                   &view->details->hover_path,
                                   NULL, NULL, NULL);

    if ((old_hover_path != NULL) != (view->details->hover_path != NULL))
    {
        if (view->details->hover_path != NULL)
        {
            gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
        }
        else
        {
            gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
        }
    }

    if (old_hover_path != NULL)
    {
        gtk_tree_path_free (old_hover_path);
    }
}

static void
on_event_controller_motion_leave (GtkEventControllerMotion *controller,
                                  gpointer                  user_data)
{
    NautilusListView *view;

    view = user_data;

    if (get_click_policy () != NAUTILUS_CLICK_POLICY_SINGLE ||
        view->details->hover_path == NULL)
    {
        return;
    }

    gtk_tree_path_free (view->details->hover_path);
    view->details->hover_path = NULL;
}

static void
on_event_controller_motion_enter (GtkEventControllerMotion *controller,
                                  double                    x,
                                  double                    y,
                                  gpointer                  user_data)
{
    NautilusListView *view;
    GtkWidget *widget;
    int x_in_bin;
    int y_in_bin;

    if (get_click_policy () != NAUTILUS_CLICK_POLICY_SINGLE)
    {
        return;
    }

    view = user_data;
    if (view->details->hover_path != NULL)
    {
        gtk_tree_path_free (view->details->hover_path);
    }
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));

    gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (widget),
                                                       x, y,
                                                       &x_in_bin, &y_in_bin);
    gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                   x_in_bin, y_in_bin,
                                   &view->details->hover_path,
                                   NULL, NULL, NULL);

    if (view->details->hover_path != NULL)
    {
        gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
    }
}

static void
row_activated_callback (GtkTreeView       *treeview,
                        GtkTreePath       *path,
                        GtkTreeViewColumn *column,
                        NautilusListView  *view)
{
    activate_selected_items (view);
}

static gboolean
check_starred_status (GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      data)
{
    NautilusFile *file;
    GList *l;
    GList *changed_files;

    changed_files = data;

    gtk_tree_model_get (GTK_TREE_MODEL (model),
                        iter,
                        NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    if (!file)
    {
        return FALSE;
    }

    for (l = changed_files; l != NULL; l = l->next)
    {
        if (nautilus_file_compare_location (NAUTILUS_FILE (l->data), file) == 0)
        {
            gtk_tree_model_row_changed (model, path, iter);
        }
    }

    nautilus_file_unref (file);

    return FALSE;
}

static void
on_starred_files_changed (NautilusTagManager *tag_manager,
                          GList              *changed_files,
                          gpointer            user_data)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (user_data);

    gtk_tree_model_foreach (GTK_TREE_MODEL (list_view->details->model),
                            check_starred_status,
                            changed_files);
}

static void
on_star_cell_renderer_clicked (GtkTreePath      *path,
                               NautilusListView *list_view)
{
    NautilusListModel *list_model;
    NautilusFile *file;
    g_autofree gchar *uri = NULL;
    GList *selection;

    list_model = list_view->details->model;

    file = nautilus_list_model_file_for_path (list_model, path);

    if (file == NULL)
    {
        /* This row is a label, not a file */
        return;
    }

    uri = nautilus_file_get_uri (file);
    selection = g_list_prepend (NULL, file);

    if (nautilus_tag_manager_file_is_starred (list_view->details->tag_manager, uri))
    {
        nautilus_tag_manager_unstar_files (list_view->details->tag_manager,
                                           G_OBJECT (list_view),
                                           selection,
                                           NULL,
                                           list_view->details->starred_cancellable);
    }
    else
    {
        nautilus_tag_manager_star_files (list_view->details->tag_manager,
                                         G_OBJECT (list_view),
                                         selection,
                                         NULL,
                                         list_view->details->starred_cancellable);
    }

    nautilus_file_list_free (selection);
}

static void
on_tree_view_multi_press_gesture_pressed (GtkGestureMultiPress *gesture,
                                          gint                  n_press,
                                          gdouble               x,
                                          gdouble               y,
                                          gpointer              callback_data)
{
    NautilusListView *view;
    GtkWidget *widget;
    GtkTreeView *tree_view;
    g_autoptr (GtkTreePath) path = NULL;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    guint button;
    gint bin_x;
    gint bin_y;
    GdkEventSequence *sequence;
    const GdkEvent *event;
    gboolean on_expander, show_expanders;
    gboolean is_simple_click, path_selected;
    NautilusFile *file;
    gboolean on_star;

    view = NAUTILUS_LIST_VIEW (callback_data);
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    tree_view = GTK_TREE_VIEW (widget);
    selection = gtk_tree_view_get_selection (tree_view);
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, x, y, &bin_x, &bin_y);

    view->details->last_event_button_x = bin_x;
    view->details->last_event_button_y = bin_y;

    /* Don't handle extra mouse buttons here */
    if (button > 5)
    {
        return;
    }

    sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
    event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);

    /* Column headers lie above bin_window, hence negative y coordinate. */
    if (bin_y < 0)
    {
        if (button == GDK_BUTTON_SECONDARY)
        {
            popup_column_header_menu (view, x, y);
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        }
        else
        {
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
        }
        return;
    }

    nautilus_list_model_set_drag_view
        (NAUTILUS_LIST_MODEL (gtk_tree_view_get_model (tree_view)),
        tree_view,
        bin_x, bin_y);

    /* Ignore double click if we are in single click mode */
    if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE && n_press >= 2)
    {
        return;
    }

    is_simple_click = ((button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_MIDDLE) && (n_press == 1));

    /* No item at this position */
    if (!gtk_tree_view_get_path_at_pos (tree_view, bin_x, bin_y,
                                        &path, &column, NULL, NULL))
    {
        if (is_simple_click)
        {
            g_clear_pointer (&view->details->first_click_path, gtk_tree_path_free);
        }

        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree_view));

        if (button == GDK_BUTTON_SECONDARY)
        {
            nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (view),
                                                                x, y);
        }

        return;
    }

    on_expander = FALSE;
    path_selected = gtk_tree_selection_path_is_selected (selection, path);
    show_expanders = g_settings_get_boolean (nautilus_list_view_preferences,
                                             NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);

    if (show_expanders)
    {
        GdkRectangle cell_area;

        gtk_tree_view_get_cell_area (tree_view, path, column, &cell_area);

        /* We assume that the cell area excludes the expander itself.
         * Explanatory link for future reference:
         * https://gitlab.gnome.org/GNOME/nautilus/merge_requests/97#note_58649 */

        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
        {
            on_expander = bin_x > (cell_area.x + cell_area.width);
        }
        else
        {
            on_expander = bin_x < cell_area.x;
        }
    }

    /* Keep track of path of last click so double clicks only happen
     * on the same item */
    if (is_simple_click)
    {
        g_clear_pointer (&view->details->first_click_path, gtk_tree_path_free);
        view->details->first_click_path = gtk_tree_path_copy (path);
    }

    on_star = (g_hash_table_lookup (view->details->columns, "starred") == column &&
               !gtk_tree_view_is_blank_at_pos (tree_view,
                                               bin_x,
                                               bin_y,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL));

    if (is_simple_click && on_star)
    {
        on_star_cell_renderer_clicked (path, view);
    }
    else if (n_press == 2 && !on_star)
    {
        /* Double clicking does not trigger a D&D action. */
        view->details->drag_button = 0;

        /* NOTE: Activation can actually destroy the view if we're switching */
        if (!on_expander &&
            view->details->first_click_path &&
            gtk_tree_path_compare (path, view->details->first_click_path) == 0)
        {
            if ((button == GDK_BUTTON_PRIMARY) && button_event_modifies_selection (event))
            {
                file = nautilus_list_model_file_for_path (view->details->model, path);
                if (file != NULL)
                {
                    activate_selected_items_alternate (view, file, TRUE);
                    nautilus_file_unref (file);
                }
            }
            else if ((button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_SECONDARY))
            {
                activate_selected_items (view);
            }
        }
        else
        {
            return;
        }
    }
    else
    {
        GdkModifierType state;
        g_autoptr (GtkTreePath) cursor = NULL;
        GList *selected_rows = NULL;

        gdk_event_get_state (event, &state);

        /* We cannot easily match the expected behavior of Shift+click, so we
         * must fall back to GtkTreeView's default event handling.
         *
         * If Shift and Ctrl are held simultateously, GtkTreeView ignores Shift,
         * so we implement a more useful behavior ourselves.
         */
        if ((state & GDK_SHIFT_MASK) != 0 && (state & GDK_CONTROL_MASK) == 0)
        {
            return;
        }

        /* Let GtkTreeView handle tree expanding/collapsing. */
        if (is_simple_click && on_expander)
        {
            return;
        }

        /* As we don't let GtkTreeView default event handling go through, so we
         * must grab the focus ourselves. */
        gtk_widget_grab_focus (widget);

        if (!path_selected)
        {
            if ((state & GDK_CONTROL_MASK) != 0)
            {
                if ((state & GDK_SHIFT_MASK) != 0)
                {
                    gtk_tree_view_get_cursor (tree_view, &cursor, NULL);
                    if (cursor != NULL)
                    {
                        gtk_tree_selection_select_range (selection, cursor, path);
                    }
                    else
                    {
                        gtk_tree_selection_select_path (selection, path);
                    }
                }
                else
                {
                    gtk_tree_selection_select_path (selection, path);
                }
                selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);

                /* This unselects everything */
                gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

                /* So select it again */
                for (GList *l = selected_rows; l != NULL; l = l->next)
                {
                    gtk_tree_selection_select_path (selection, l->data);
                }
                g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
            }
            else
            {
                gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
            }
        }

        if (is_simple_click)
        {
            view->details->drag_started = FALSE;
            view->details->drag_button = button;
            view->details->drag_x = bin_x;
            view->details->drag_y = bin_y;
            view->details->row_selected_on_button_down = path_selected;
        }

        if (button == GDK_BUTTON_SECONDARY)
        {
            nautilus_files_view_pop_up_selection_context_menu (NAUTILUS_FILES_VIEW (view),
                                                               x, y);
        }

        /* Don't open a new tab if we are in single click mode (this would open 2 tabs),
         * or if CTRL or SHIFT is pressed.
         */
        if (button == GDK_BUTTON_MIDDLE &&
            get_click_policy () != NAUTILUS_CLICK_POLICY_SINGLE &&
            !button_event_modifies_selection (event))
        {
            gtk_tree_selection_unselect_all (selection);
            gtk_tree_selection_select_path (selection, path);

            activate_selected_items_alternate (view, NULL, TRUE);
        }
    }

    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_tree_view_multi_press_gesture_released (GtkGestureMultiPress *gesture,
                                           gint                  n_press,
                                           gdouble               x,
                                           gdouble               y,
                                           gpointer              callback_data)
{
    NautilusListView *view;
    guint button;
    GdkEventSequence *sequence;
    const GdkEvent *event;
    GtkTreeView *tree_view;
    GtkTreeSelection *selection;
    gint x_in_bin;
    gint y_in_bin;
    GtkTreePath *path;
    GdkModifierType state;

    view = NAUTILUS_LIST_VIEW (callback_data);
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    if (button != view->details->drag_button)
    {
        return;
    }

    view->details->drag_button = 0;

    if (view->details->drag_started)
    {
        return;
    }

    /* Did not drag. */

    sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
    event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
    /* Typically will only happen with GTK+ <= 3.22.30 and <= 3.93.0,
     * where ::released is emitted after ::cancel, but can’t hurt to guard
     * against it anyway.
     */
    if (event == NULL)
    {
        return;
    }

    tree_view = view->details->tree_view;
    selection = gtk_tree_view_get_selection (tree_view);


    gtk_tree_view_convert_widget_to_bin_window_coords (tree_view,
                                                       x, y,
                                                       &x_in_bin, &y_in_bin);

    if (!gtk_tree_view_get_path_at_pos (tree_view, x_in_bin, y_in_bin, &path, NULL, NULL, NULL))
    {
        return;
    }

    gdk_event_get_state (event, &state);

    if ((button == GDK_BUTTON_PRIMARY || button == GDK_BUTTON_MIDDLE)
        && ((state & GDK_CONTROL_MASK) != 0 ||
            (state & GDK_SHIFT_MASK) == 0)
        && view->details->row_selected_on_button_down)
    {
        if (!button_event_modifies_selection (event))
        {
            gtk_tree_selection_unselect_all (selection);
            gtk_tree_selection_select_path (selection, path);
        }
        else
        {
            gtk_tree_selection_unselect_path (selection, path);
        }
    }

    if ((get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE)
        && !button_event_modifies_selection (event))
    {
        if (button == GDK_BUTTON_PRIMARY)
        {
            activate_selected_items (view);
        }
        else if (button == GDK_BUTTON_MIDDLE)
        {
            activate_selected_items_alternate (view, NULL, TRUE);
        }
    }
    gtk_tree_path_free (path);
}

static gboolean
on_event_controller_key_key_pressed (GtkEventControllerKey *controller,
                                     unsigned int           keyval,
                                     unsigned int           keycode,
                                     GdkModifierType        state,
                                     gpointer               user_data)
{
    GtkWidget *widget;
    NautilusFilesView *view;
    GtkTreeView *tree_view;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
    view = NAUTILUS_FILES_VIEW (user_data);
    tree_view = GTK_TREE_VIEW (widget);

    NAUTILUS_LIST_VIEW (view)->details->last_event_button_x = -1;
    NAUTILUS_LIST_VIEW (view)->details->last_event_button_y = -1;

    if (keyval == GDK_KEY_F10)
    {
        if ((state & GDK_CONTROL_MASK) != 0)
        {
            nautilus_files_view_pop_up_background_context_menu (view, 0, 0);

            return GDK_EVENT_STOP;
        }
    }

    if (keyval == GDK_KEY_Right)
    {
        g_autoptr (GtkTreePath) path = NULL;

        gtk_tree_view_get_cursor (tree_view, &path, NULL);

        if (path != NULL)
        {
            gtk_tree_view_expand_row (tree_view, path, FALSE);
        }

        return GDK_EVENT_STOP;
    }

    if (keyval == GDK_KEY_Left)
    {
        g_autoptr (GtkTreePath) path = NULL;

        gtk_tree_view_get_cursor (tree_view, &path, NULL);

        if (path != NULL && !gtk_tree_view_collapse_row (tree_view, path))
        {
            /* if the row is already collapsed or doesn't have any children,
             * jump to the parent row instead.
             */
            if ((gtk_tree_path_get_depth (path) > 1) && gtk_tree_path_up (path))
            {
                gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
            }
        }

        return GDK_EVENT_STOP;
    }

    if (keyval == GDK_KEY_space)
    {
        if ((state & GDK_CONTROL_MASK) != 0)
        {
            return GDK_EVENT_PROPAGATE;
        }

        if (!gtk_widget_has_focus (GTK_WIDGET (NAUTILUS_LIST_VIEW (view)->details->tree_view)))
        {
            return GDK_EVENT_PROPAGATE;
        }

        if ((state & GDK_SHIFT_MASK) != 0)
        {
            activate_selected_items_alternate (NAUTILUS_LIST_VIEW (view), NULL, TRUE);
        }

        return GDK_EVENT_STOP;
    }

    if (keyval == GDK_KEY_v)
    {
        /* Eat Control + v to not enable type ahead */
        if ((state & GDK_CONTROL_MASK) != 0)
        {
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static void
subdirectory_done_loading_callback (NautilusDirectory *directory,
                                    NautilusListView  *view)
{
    nautilus_list_model_subdirectory_done_loading (view->details->model, directory);
}

static void
row_expanded_callback (GtkTreeView *treeview,
                       GtkTreeIter *iter,
                       GtkTreePath *path,
                       gpointer     callback_data)
{
    NautilusListView *view;
    NautilusDirectory *directory;
    char *uri;

    view = NAUTILUS_LIST_VIEW (callback_data);

    if (!nautilus_list_model_load_subdirectory (view->details->model, path, &directory))
    {
        return;
    }

    uri = nautilus_directory_get_uri (directory);
    DEBUG ("Row expanded callback for URI %s", uri);
    g_free (uri);

    nautilus_files_view_add_subdirectory (NAUTILUS_FILES_VIEW (view), directory);

    if (nautilus_directory_are_all_files_seen (directory))
    {
        nautilus_list_model_subdirectory_done_loading (view->details->model,
                                                       directory);
    }
    else
    {
        g_signal_connect_object (directory, "done-loading",
                                 G_CALLBACK (subdirectory_done_loading_callback),
                                 view, 0);
    }

    nautilus_directory_unref (directory);
}

typedef struct
{
    NautilusFile *file;
    NautilusDirectory *directory;
    NautilusListView *view;
} UnloadDelayData;

static void
unload_delay_data_free (UnloadDelayData *unload_data)
{
    if (unload_data->view != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (unload_data->view),
                                      (gpointer *) &unload_data->view);
    }

    nautilus_directory_unref (unload_data->directory);
    nautilus_file_unref (unload_data->file);

    g_slice_free (UnloadDelayData, unload_data);
}

static UnloadDelayData *
unload_delay_data_new (NautilusFile      *file,
                       NautilusDirectory *parent_directory,
                       NautilusListView  *view)
{
    UnloadDelayData *unload_data;

    unload_data = g_slice_new0 (UnloadDelayData);
    unload_data->view = view;
    unload_data->file = nautilus_file_ref (file);
    unload_data->directory = nautilus_directory_ref (parent_directory);

    g_object_add_weak_pointer (G_OBJECT (unload_data->view),
                               (gpointer *) &unload_data->view);

    return unload_data;
}

static gboolean
unload_file_timeout (gpointer data)
{
    UnloadDelayData *unload_data = data;
    GtkTreeIter iter;
    NautilusListModel *model;
    GtkTreePath *path;

    if (unload_data->view == NULL)
    {
        goto out;
    }

    model = unload_data->view->details->model;
    if (nautilus_list_model_get_tree_iter_from_file (model,
                                                     unload_data->file,
                                                     unload_data->directory,
                                                     &iter))
    {
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
        if (!gtk_tree_view_row_expanded (unload_data->view->details->tree_view,
                                         path))
        {
            nautilus_list_model_unload_subdirectory (model, &iter);
        }
        gtk_tree_path_free (path);
    }

out:
    unload_delay_data_free (unload_data);
    return FALSE;
}

static void
row_collapsed_callback (GtkTreeView *treeview,
                        GtkTreeIter *iter,
                        GtkTreePath *path,
                        gpointer     callback_data)
{
    NautilusListView *view;
    NautilusFile *file;
    NautilusDirectory *directory;
    GtkTreeIter parent;
    UnloadDelayData *unload_data;
    GtkTreeModel *model;
    char *uri;

    view = NAUTILUS_LIST_VIEW (callback_data);
    model = GTK_TREE_MODEL (view->details->model);

    gtk_tree_model_get (model, iter,
                        NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    uri = nautilus_file_get_uri (file);
    DEBUG ("Row collapsed callback for uri %s", uri);
    g_free (uri);

    directory = NULL;
    if (gtk_tree_model_iter_parent (model, &parent, iter))
    {
        gtk_tree_model_get (model, &parent,
                            NAUTILUS_LIST_MODEL_SUBDIRECTORY_COLUMN, &directory,
                            -1);
    }

    unload_data = unload_delay_data_new (file, directory, view);
    g_timeout_add_seconds (COLLAPSE_TO_UNLOAD_DELAY,
                           unload_file_timeout,
                           unload_data);

    nautilus_file_unref (file);
    nautilus_directory_unref (directory);
}

static void
subdirectory_unloaded_callback (NautilusListModel *model,
                                NautilusDirectory *directory,
                                gpointer           callback_data)
{
    NautilusListView *view;

    g_return_if_fail (NAUTILUS_IS_LIST_MODEL (model));
    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

    view = NAUTILUS_LIST_VIEW (callback_data);

    g_signal_handlers_disconnect_by_func (directory,
                                          G_CALLBACK (subdirectory_done_loading_callback),
                                          view);
    nautilus_files_view_remove_subdirectory (NAUTILUS_FILES_VIEW (view), directory);
}

static gboolean
test_expand_row_callback (GtkTreeView *tree_view,
                          GtkTreeIter *iter,
                          GtkTreePath *path,
                          gboolean     user_data)
{
    return !g_settings_get_boolean (nautilus_list_view_preferences,
                                    NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);
}

static void
nautilus_list_view_reveal_selection (NautilusFilesView *view)
{
    g_autolist (NautilusFile) selection = NULL;

    g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));

    /* Make sure at least one of the selected items is scrolled into view */
    if (selection != NULL)
    {
        NautilusListView *list_view;
        NautilusFile *file;
        GtkTreeIter iter;
        GtkTreePath *path;

        list_view = NAUTILUS_LIST_VIEW (view);
        file = selection->data;
        if (nautilus_list_model_get_first_iter_for_file (list_view->details->model, file, &iter))
        {
            path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

            gtk_tree_view_scroll_to_cell (list_view->details->tree_view, path, NULL, FALSE, 0.0, 0.0);

            gtk_tree_path_free (path);
        }
    }
}

static gboolean
sort_criterion_changes_due_to_user (GtkTreeView *tree_view)
{
    GList *columns, *p;
    GtkTreeViewColumn *column;
    GSignalInvocationHint *ihint;
    gboolean ret;

    ret = FALSE;

    columns = gtk_tree_view_get_columns (tree_view);
    for (p = columns; p != NULL; p = p->next)
    {
        column = p->data;
        ihint = g_signal_get_invocation_hint (column);
        if (ihint != NULL)
        {
            ret = TRUE;
            break;
        }
    }
    g_list_free (columns);

    return ret;
}

static void
sort_column_changed_callback (GtkTreeSortable  *sortable,
                              NautilusListView *view)
{
    NautilusFile *file;
    gint sort_column_id, default_sort_column_id;
    GtkSortType reversed;
    GQuark sort_attr, default_sort_attr;
    char *reversed_attr, *default_reversed_attr;
    gboolean default_sort_reversed;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (view));

    gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &reversed);
    sort_attr = nautilus_list_model_get_attribute_from_sort_column_id (view->details->model, sort_column_id);

    default_sort_column_id = nautilus_list_model_get_sort_column_id_from_attribute (view->details->model,
                                                                                    g_quark_from_string (get_default_sort_order (file, &default_sort_reversed)));
    default_sort_attr = nautilus_list_model_get_attribute_from_sort_column_id (view->details->model, default_sort_column_id);
    nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
                                g_quark_to_string (default_sort_attr), g_quark_to_string (sort_attr));

    default_reversed_attr = (default_sort_reversed ? "true" : "false");

    if (view->details->last_sort_attr != sort_attr &&
        sort_criterion_changes_due_to_user (view->details->tree_view))
    {
        /* at this point, the sort order is always GTK_SORT_ASCENDING, if the sort column ID
         * switched. Invert the sort order, if it's the default criterion with a reversed preference,
         * or if it makes sense for the attribute (i.e. date). */
        if (sort_attr == default_sort_attr)
        {
            /* use value from preferences */
            reversed = g_settings_get_boolean (nautilus_preferences,
                                               NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);
        }
        else
        {
            reversed = nautilus_file_is_date_sort_attribute_q (sort_attr);
        }

        if (reversed)
        {
            g_signal_handlers_block_by_func (sortable, sort_column_changed_callback, view);
            gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (view->details->model),
                                                  sort_column_id,
                                                  GTK_SORT_DESCENDING);
            g_signal_handlers_unblock_by_func (sortable, sort_column_changed_callback, view);
        }
    }


    reversed_attr = (reversed ? "true" : "false");
    nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
                                default_reversed_attr, reversed_attr);

    /* Make sure selected item(s) is visible after sort */
    nautilus_list_view_reveal_selection (NAUTILUS_FILES_VIEW (view));

    view->details->last_sort_attr = sort_attr;
}

static char *
get_root_uri_callback (NautilusTreeViewDragDest *dest,
                       gpointer                  user_data)
{
    NautilusListView *view;

    view = NAUTILUS_LIST_VIEW (user_data);

    return nautilus_files_view_get_uri (NAUTILUS_FILES_VIEW (view));
}

static NautilusFile *
get_file_for_path_callback (NautilusTreeViewDragDest *dest,
                            GtkTreePath              *path,
                            gpointer                  user_data)
{
    NautilusListView *view;

    view = NAUTILUS_LIST_VIEW (user_data);

    return nautilus_list_model_file_for_path (view->details->model, path);
}


static void
list_view_handle_uri_list (NautilusTreeViewDragDest *dest,
                           const char               *item_uris,
                           const char               *target_uri,
                           GdkDragAction             action,
                           NautilusListView         *view)
{
    nautilus_files_view_handle_uri_list_drop (NAUTILUS_FILES_VIEW (view),
                                              item_uris, target_uri, action);
}

static void
list_view_handle_text (NautilusTreeViewDragDest *dest,
                       const char               *text,
                       const char               *target_uri,
                       GdkDragAction             action,
                       NautilusListView         *view)
{
    nautilus_files_view_handle_text_drop (NAUTILUS_FILES_VIEW (view),
                                          text, target_uri, action);
}

static void
list_view_handle_raw (NautilusTreeViewDragDest *dest,
                      const char               *raw_data,
                      int                       length,
                      const char               *target_uri,
                      const char               *direct_save_uri,
                      GdkDragAction             action,
                      NautilusListView         *view)
{
    nautilus_files_view_handle_raw_drop (NAUTILUS_FILES_VIEW (view),
                                         raw_data, length, target_uri, direct_save_uri,
                                         action);
}

static void
list_view_handle_hover (NautilusTreeViewDragDest *dest,
                        const char               *target_uri,
                        NautilusListView         *view)
{
    nautilus_files_view_handle_hover (NAUTILUS_FILES_VIEW (view), target_uri);
}

static void
move_copy_items_callback (NautilusTreeViewDragDest *dest,
                          const GList              *item_uris,
                          const char               *target_uri,
                          guint                     action,
                          gpointer                  user_data)
{
    NautilusFilesView *view = user_data;

    nautilus_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
                                                item_uris);
    nautilus_files_view_move_copy_items (view,
                                         item_uris,
                                         target_uri,
                                         action);
}

static void
column_header_menu_toggled (GtkCheckButton   *menu_item,
                            NautilusListView *list_view)
{
    NautilusFile *file;
    char **visible_columns;
    char **column_order;
    const char *column;
    GList *list = NULL;
    GList *l;
    int i;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));
    visible_columns = get_visible_columns (list_view);
    column_order = get_column_order (list_view);
    column = g_object_get_data (G_OBJECT (menu_item), "column-name");

    for (i = 0; visible_columns[i] != NULL; ++i)
    {
        list = g_list_prepend (list, visible_columns[i]);
    }

    if (gtk_check_button_get_active (menu_item))
    {
        list = g_list_prepend (list, g_strdup (column));
    }
    else
    {
        l = g_list_find_custom (list, column, (GCompareFunc) g_strcmp0);
        list = g_list_delete_link (list, l);
    }

    list = g_list_reverse (list);
    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                     list);

    g_free (visible_columns);

    visible_columns = g_new0 (char *, g_list_length (list) + 1);
    for (i = 0, l = list; l != NULL; ++i, l = l->next)
    {
        visible_columns[i] = l->data;
    }

    /* set view values ourselves, as new metadata could not have been
     * updated yet.
     */
    apply_columns_settings (list_view, column_order, visible_columns);

    g_list_free (list);
    g_strfreev (column_order);
    g_strfreev (visible_columns);
}

static void
column_header_menu_use_default (GtkButton        *menu_item,
                                NautilusListView *list_view)
{
    NautilusFile *file;
    char **default_columns;
    char **default_order;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));

    nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
    nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

    default_columns = get_default_visible_columns (list_view);
    default_order = get_default_column_order (list_view);

    /* set view values ourselves, as new metadata could not have been
     * updated yet.
     */
    apply_columns_settings (list_view, default_order, default_columns);
    /* Popdown the popover because the checkboxes are not updated. */
    gtk_popover_popdown (GTK_POPOVER (list_view->details->columns_popover));

    g_strfreev (default_columns);
    g_strfreev (default_order);
}

static void
popup_column_header_menu (NautilusListView *list_view,
                          gdouble           x,
                          gdouble           y)
{
    NautilusFile *file;
    char **visible_columns;
    char **column_order;
    GList *all_columns;
    GHashTable *visible_columns_hash;
    int i;
    GList *l;
    GtkPopover *popover;
    GtkWidget *menu;
    GtkWidget *menu_item;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));

    visible_columns = get_visible_columns (list_view);
    column_order = get_column_order (list_view);

    all_columns = nautilus_get_columns_for_file (file);
    all_columns = nautilus_sort_columns (all_columns, column_order);

    /* hash table to lookup if a given column should be visible */
    visible_columns_hash = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) g_free);
    /* always show name column */
    g_hash_table_insert (visible_columns_hash, g_strdup ("name"), g_strdup ("name"));
    if (visible_columns != NULL)
    {
        for (i = 0; visible_columns[i] != NULL; ++i)
        {
            g_hash_table_insert (visible_columns_hash,
                                 g_ascii_strdown (visible_columns[i], -1),
                                 g_ascii_strdown (visible_columns[i], -1));
        }
    }

    popover = GTK_POPOVER (list_view->details->columns_popover);
    menu = list_view->details->columns_popover_box;
    /* Remove all old items before repopulating. */
    while ((menu_item = gtk_widget_get_first_child (menu)) != NULL)
    {
        gtk_box_remove (GTK_BOX (menu), menu_item);
    }

    for (l = all_columns; l != NULL; l = l->next)
    {
        char *name;
        char *label;
        char *lowercase;

        g_object_get (G_OBJECT (l->data),
                      "name", &name,
                      "label", &label,
                      NULL);
        lowercase = g_ascii_strdown (name, -1);

        menu_item = gtk_check_button_new_with_label (label);
        gtk_box_append (GTK_BOX (menu), menu_item);

        g_object_set_data_full (G_OBJECT (menu_item),
                                "column-name", name, g_free);

        /* name is always visible */
        if (strcmp (lowercase, "name") == 0)
        {
            gtk_widget_set_sensitive (menu_item, FALSE);
        }

        if (g_hash_table_lookup (visible_columns_hash, lowercase) != NULL)
        {
            gtk_check_button_set_active (GTK_CHECK_BUTTON (menu_item), TRUE);
        }

        g_signal_connect (menu_item,
                          "toggled",
                          G_CALLBACK (column_header_menu_toggled),
                          list_view);

        g_free (lowercase);
        g_free (label);
    }

    menu_item = gtk_button_new_with_label (_("Use Default"));
    gtk_box_append (GTK_BOX (menu), menu_item);

    g_signal_connect (menu_item,
                      "clicked",
                      G_CALLBACK (column_header_menu_use_default),
                      list_view);

    gtk_widget_show_all (menu);
    gtk_popover_set_pointing_to (popover, &(GdkRectangle){x, y, 0, 0});
    gtk_popover_popup (popover);

    g_hash_table_destroy (visible_columns_hash);
    nautilus_column_list_free (all_columns);
    g_strfreev (column_order);
    g_strfreev (visible_columns);
}

static void
apply_columns_settings (NautilusListView  *list_view,
                        char             **column_order,
                        char             **visible_columns)
{
    GList *all_columns;
    NautilusFile *file;
    GList *old_view_columns, *view_columns;
    GHashTable *visible_columns_hash;
    GtkTreeViewColumn *prev_view_column;
    GList *l;
    int i;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));

    /* prepare ordered list of view columns using column_order and visible_columns */
    view_columns = NULL;

    all_columns = nautilus_get_columns_for_file (file);
    all_columns = nautilus_sort_columns (all_columns, column_order);

    /* hash table to lookup if a given column should be visible */
    visible_columns_hash = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) g_free);
    /* always show name column */
    g_hash_table_insert (visible_columns_hash, g_strdup ("name"), g_strdup ("name"));
    if (visible_columns != NULL)
    {
        for (i = 0; visible_columns[i] != NULL; ++i)
        {
            g_hash_table_insert (visible_columns_hash,
                                 g_ascii_strdown (visible_columns[i], -1),
                                 g_ascii_strdown (visible_columns[i], -1));
        }
    }

    for (l = all_columns; l != NULL; l = l->next)
    {
        char *name;
        char *lowercase;

        g_object_get (G_OBJECT (l->data), "name", &name, NULL);
        lowercase = g_ascii_strdown (name, -1);

        if (g_hash_table_lookup (visible_columns_hash, lowercase) != NULL)
        {
            GtkTreeViewColumn *view_column;

            view_column = g_hash_table_lookup (list_view->details->columns, name);
            if (view_column != NULL)
            {
                view_columns = g_list_prepend (view_columns, view_column);
            }
        }

        g_free (name);
        g_free (lowercase);
    }

    g_hash_table_destroy (visible_columns_hash);
    nautilus_column_list_free (all_columns);

    view_columns = g_list_reverse (view_columns);

    /* hide columns that are not present in the configuration */
    old_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);
    for (l = old_view_columns; l != NULL; l = l->next)
    {
        if (g_list_find (view_columns, l->data) == NULL)
        {
            gtk_tree_view_column_set_visible (l->data, FALSE);
        }
    }
    g_list_free (old_view_columns);

    /* show new columns from the configuration */
    for (l = view_columns; l != NULL; l = l->next)
    {
        gtk_tree_view_column_set_visible (l->data, TRUE);
    }

    /* place columns in the correct order */
    prev_view_column = NULL;
    for (l = view_columns; l != NULL; l = l->next)
    {
        gtk_tree_view_move_column_after (list_view->details->tree_view, l->data, prev_view_column);
        prev_view_column = l->data;
    }
    g_list_free (view_columns);
}

static void
starred_cell_data_func (GtkTreeViewColumn *column,
                        GtkCellRenderer   *renderer,
                        GtkTreeModel      *model,
                        GtkTreeIter       *iter,
                        NautilusListView  *view)
{
    g_autofree gchar *text = NULL;
    g_autofree gchar *uri = NULL;
    NautilusFile *file;

    gtk_tree_model_get (model, iter,
                        view->details->file_name_column_num, &text,
                        -1);

    gtk_tree_model_get (GTK_TREE_MODEL (model),
                        iter,
                        NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    if (file == NULL)
    {
        /* This row is a label, not a file */
        g_object_set (renderer,
                      "icon-name", NULL,
                      "mode", GTK_CELL_RENDERER_MODE_INERT,
                      NULL);
        return;
    }

    uri = nautilus_file_get_uri (file);

    if (nautilus_tag_manager_file_is_starred (view->details->tag_manager, uri))
    {
        g_object_set (renderer,
                      "icon-name", "starred-symbolic",
                      NULL);
    }
    else
    {
        g_object_set (renderer,
                      "icon-name", "non-starred-symbolic",
                      NULL);
    }

    nautilus_file_unref (file);
}

static void
filename_cell_data_func (GtkTreeViewColumn *column,
                         GtkCellRenderer   *renderer,
                         GtkTreeModel      *model,
                         GtkTreeIter       *iter,
                         NautilusListView  *view)
{
    char *text;
    g_autofree gchar *escaped_text = NULL;
    g_autofree gchar *escaped_name = NULL;
    g_autofree gchar *replaced_text = NULL;
    GtkTreePath *path;
    PangoUnderline underline;
    GString *display_text;
    NautilusDirectory *directory;
    NautilusQuery *query = NULL;
    NautilusFile *file;
    const gchar *snippet;

    gtk_tree_model_get (model, iter,
                        view->details->file_name_column_num, &text,
                        -1);

    escaped_name = g_markup_escape_text (text, -1);
    display_text = g_string_new (escaped_name);

    directory = nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (view));

    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
    }

    if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE)
    {
        path = gtk_tree_model_get_path (model, iter);

        if (view->details->hover_path == NULL ||
            gtk_tree_path_compare (path, view->details->hover_path))
        {
            underline = PANGO_UNDERLINE_NONE;
        }
        else
        {
            underline = PANGO_UNDERLINE_SINGLE;
        }

        gtk_tree_path_free (path);
    }
    else
    {
        underline = PANGO_UNDERLINE_NONE;
    }

    if (query &&
        nautilus_query_get_search_content (query) == NAUTILUS_QUERY_SEARCH_CONTENT_FULL_TEXT)
    {
        gtk_tree_model_get (model, iter,
                            NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                            -1);

        /* Rule out dummy row */
        if (file != NULL)
        {
            snippet = nautilus_file_get_search_fts_snippet (file);
            if (snippet)
            {
                replaced_text = g_regex_replace (view->details->regex,
                                                 snippet,
                                                 -1,
                                                 0,
                                                 " ",
                                                 G_REGEX_MATCH_NEWLINE_ANY,
                                                 NULL);

                escaped_text = g_markup_escape_text (replaced_text, -1);

                g_string_append_printf (display_text,
                                        " <small><span alpha='50%%'><b>%s</b></span></small>",
                                        escaped_text);
            }
        }
        nautilus_file_unref (file);
    }

    g_object_set (G_OBJECT (renderer),
                  "markup", display_text->str,
                  "underline", underline,
                  NULL);

    g_free (text);
    g_string_free (display_text, TRUE);
}

static void
location_cell_data_func (GtkTreeViewColumn *column,
                         GtkCellRenderer   *renderer,
                         GtkTreeModel      *model,
                         GtkTreeIter       *iter,
                         NautilusListView  *view,
                         gboolean           show_trash_orig)
{
    NautilusDirectory *directory;
    GFile *home_location;
    NautilusFile *file;
    GFile *dir_location;
    GFile *base_location;
    gchar *where = NULL;

    directory = nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (view));

    home_location = g_file_new_for_path (g_get_home_dir ());

    gtk_tree_model_get (model, iter,
                        NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    /* The file might be NULL if we just toggled an expander
     * and we're still loading the subdirectory.
     */
    if (file == NULL)
    {
        return;
    }

    if (show_trash_orig && nautilus_file_is_in_trash (file))
    {
        NautilusFile *orig_file;

        orig_file = nautilus_file_get_trash_original_file (file);

        if (orig_file != NULL)
        {
            nautilus_file_unref (file);
            file = orig_file;
        }
    }

    if (!nautilus_file_is_in_recent (file))
    {
        dir_location = nautilus_file_get_parent_location (file);
    }
    else
    {
        GFile *activation_location;

        activation_location = nautilus_file_get_activation_location (file);
        dir_location = g_file_get_parent (activation_location);

        g_object_unref (activation_location);
    }

    if (!NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        base_location = g_object_ref (home_location);
    }
    else
    {
        NautilusQuery *query;
        NautilusFile *base;
        GFile *location;

        query = nautilus_search_directory_get_query (NAUTILUS_SEARCH_DIRECTORY (directory));
        location = nautilus_query_get_location (query);
        base = nautilus_file_get (location);

        if (!nautilus_file_is_in_recent (base))
        {
            base_location = nautilus_file_get_location (base);
        }
        else
        {
            base_location = g_object_ref (home_location);
        }

        nautilus_file_unref (base);
        g_object_unref (location);
        g_object_unref (query);
    }

    if (g_file_equal (base_location, dir_location))
    {
        /* Only occurs when search result is
         * a direct child of the base location
         */
        where = g_strdup ("");
    }
    else if (g_file_equal (home_location, dir_location))
    {
        where = g_strdup (_("Home"));
    }
    else if (g_file_has_prefix (dir_location, base_location))
    {
        gchar *relative_path;

        relative_path = g_file_get_relative_path (base_location, dir_location);
        where = g_filename_display_name (relative_path);

        g_free (relative_path);
    }
    else
    {
        where = g_file_get_path (dir_location);
    }

    g_object_set (G_OBJECT (renderer),
                  "text", where,
                  NULL);

    g_free (where);

    g_object_unref (base_location);
    g_object_unref (dir_location);
    nautilus_file_unref (file);
    g_object_unref (home_location);
}


static void
where_cell_data_func (GtkTreeViewColumn *column,
                      GtkCellRenderer   *renderer,
                      GtkTreeModel      *model,
                      GtkTreeIter       *iter,
                      NautilusListView  *view)
{
    location_cell_data_func (column, renderer, model, iter, view, FALSE);
}

static void
trash_orig_path_cell_data_func (GtkTreeViewColumn *column,
                                GtkCellRenderer   *renderer,
                                GtkTreeModel      *model,
                                GtkTreeIter       *iter,
                                NautilusListView  *view)
{
    location_cell_data_func (column, renderer, model, iter, view, TRUE);
}

#define SMALL_ZOOM_ICON_PADDING 0
#define STANDARD_ZOOM_ICON_PADDING 6
#define LARGE_ZOOM_ICON_PADDING 6
#define LARGER_ZOOM_ICON_PADDING 6

static gint
nautilus_list_view_get_icon_padding_for_zoom_level (NautilusListZoomLevel zoom_level)
{
    switch (zoom_level)
    {
        case NAUTILUS_LIST_ZOOM_LEVEL_SMALL:
        {
            return SMALL_ZOOM_ICON_PADDING;
        }

        case NAUTILUS_LIST_ZOOM_LEVEL_STANDARD:
        {
            return STANDARD_ZOOM_ICON_PADDING;
        }

        case NAUTILUS_LIST_ZOOM_LEVEL_LARGE:
        {
            return LARGE_ZOOM_ICON_PADDING;
        }

        case NAUTILUS_LIST_ZOOM_LEVEL_LARGER:
        {
            return LARGER_ZOOM_ICON_PADDING;
        }

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static void
set_up_pixbuf_size (NautilusListView *view)
{
    int icon_size, icon_padding;

    /* Make all rows the same size. */
    icon_size = nautilus_list_model_get_icon_size_for_zoom_level (view->details->zoom_level);
    icon_padding = nautilus_list_view_get_icon_padding_for_zoom_level (view->details->zoom_level);
    gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (view->details->pixbuf_cell),
                                      -1, icon_size + 2 * icon_padding);

    /* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=641518 */
    gtk_tree_view_columns_autosize (view->details->tree_view);
}

static gint
get_icon_scale_callback (NautilusListModel *model,
                         NautilusListView  *view)
{
    return gtk_widget_get_scale_factor (GTK_WIDGET (view->details->tree_view));
}

static void
on_longpress_gesture_pressed_event (GtkGestureLongPress *gesture,
                                    gdouble              x,
                                    gdouble              y,
                                    gpointer             user_data)
{
    NautilusListView *view = user_data;
    g_autolist (NautilusFile) selection = NULL;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
    if (selection != NULL)
    {
        nautilus_files_view_pop_up_selection_context_menu (NAUTILUS_FILES_VIEW (view), x, y);
    }
    else
    {
        nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (view), x, y);
    }
}

static void
on_tree_view_drag_gesture_drag_begin (GtkGestureDrag *gesture,
                                      gdouble         start_x,
                                      gdouble         start_y,
                                      gpointer        user_data)
{
    nautilus_list_view_dnd_init (NAUTILUS_LIST_VIEW (user_data));
}

static void
on_tree_view_drag_gesture_drag_update (GtkGestureDrag *gesture,
                                       gdouble         offset_x,
                                       gdouble         offset_y,
                                       gpointer        user_data)
{
    GdkEventSequence *sequence;
    const GdkEvent *event;
    NautilusListView *list_view;

    sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
    event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
    list_view = NAUTILUS_LIST_VIEW (user_data);

    nautilus_list_view_dnd_drag_begin (list_view, offset_x, offset_y, event);
}

static void
list_view_use_tree_changed_callback (gpointer callback_data)
{
    GtkTreeView *tree_view;

    tree_view = GTK_TREE_VIEW (callback_data);

    gtk_tree_view_collapse_all (tree_view);
}


static void
create_and_set_up_tree_view (NautilusListView *view)
{
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    AtkObject *atk_obj;
    GList *nautilus_columns;
    GList *l;
    gchar **default_column_order, **default_visible_columns;
    GtkWidget *content_widget;
    GtkGesture *gesture;
    GtkEventController *controller;

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (view));
    view->details->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
    view->details->columns = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    (GDestroyNotify) g_free,
                                                    NULL);
    gtk_tree_view_set_enable_search (view->details->tree_view, FALSE);

    view->details->drag_dest =
        nautilus_tree_view_drag_dest_new (view->details->tree_view);

    /* Stop the tree view from performing select-all actions.
     * It is desireable that the action is disabled while directory
     * is loading.
     */
    g_signal_connect (view->details->tree_view, "select-all",
                      G_CALLBACK (g_signal_stop_emission_by_name), "select-all");

    g_signal_connect_object (view->details->drag_dest,
                             "get-root-uri",
                             G_CALLBACK (get_root_uri_callback),
                             view, 0);
    g_signal_connect_object (view->details->drag_dest,
                             "get-file-for-path",
                             G_CALLBACK (get_file_for_path_callback),
                             view, 0);
    g_signal_connect_object (view->details->drag_dest,
                             "move-copy-items",
                             G_CALLBACK (move_copy_items_callback),
                             view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle-uri-list",
                             G_CALLBACK (list_view_handle_uri_list), view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle-text",
                             G_CALLBACK (list_view_handle_text), view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle-raw",
                             G_CALLBACK (list_view_handle_raw), view, 0);
    g_signal_connect_object (view->details->drag_dest, "handle-hover",
                             G_CALLBACK (list_view_handle_hover), view, 0);

    g_signal_connect_object (gtk_tree_view_get_selection (view->details->tree_view),
                             "changed",
                             G_CALLBACK (list_selection_changed_callback), view, 0);

    gesture = gtk_gesture_drag_new (GTK_WIDGET (view->details->tree_view));
    view->details->tree_view_drag_gesture = gesture;

    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);

    g_signal_connect (gesture, "drag-begin",
                      G_CALLBACK (on_tree_view_drag_gesture_drag_begin), view);
    g_signal_connect (gesture, "drag-update",
                      G_CALLBACK (on_tree_view_drag_gesture_drag_update), view);

    gesture = gtk_gesture_multi_press_new (GTK_WIDGET (view->details->tree_view));
    view->details->tree_view_multi_press_gesture = gesture;

    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);

    g_signal_connect (gesture, "pressed",
                      G_CALLBACK (on_tree_view_multi_press_gesture_pressed), view);
    g_signal_connect (gesture, "released",
                      G_CALLBACK (on_tree_view_multi_press_gesture_released), view);

    controller = gtk_event_controller_motion_new (GTK_WIDGET (view->details->tree_view));
    view->details->motion_controller = controller;

    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);

    g_signal_connect (controller, "enter",
                      G_CALLBACK (on_event_controller_motion_enter), view);
    g_signal_connect (controller, "leave",
                      G_CALLBACK (on_event_controller_motion_leave), view);
    g_signal_connect (controller, "motion",
                      G_CALLBACK (on_event_controller_motion_motion), view);

    controller = gtk_event_controller_key_new (GTK_WIDGET (view->details->tree_view));
    view->details->key_controller = controller;

    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);

    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (on_event_controller_key_key_pressed), view);

    g_signal_connect_object (view->details->tree_view, "test-expand-row",
                             G_CALLBACK (test_expand_row_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "row-expanded",
                             G_CALLBACK (row_expanded_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "row-collapsed",
                             G_CALLBACK (row_collapsed_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "row-activated",
                             G_CALLBACK (row_activated_callback), view, 0);

    g_signal_connect_object (nautilus_list_view_preferences,
                             "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE,
                             G_CALLBACK (list_view_use_tree_changed_callback),
                             view->details->tree_view,
                             G_CONNECT_SWAPPED);

    view->details->model = g_object_new (NAUTILUS_TYPE_LIST_MODEL, NULL);
    gtk_tree_view_set_model (view->details->tree_view, GTK_TREE_MODEL (view->details->model));
    /* Need the model for the dnd drop icon "accept" change */
    nautilus_list_model_set_drag_view (NAUTILUS_LIST_MODEL (view->details->model),
                                       view->details->tree_view, 0, 0);

    g_signal_connect_object (view->details->model, "sort-column-changed",
                             G_CALLBACK (sort_column_changed_callback), view, 0);

    g_signal_connect_object (view->details->model, "subdirectory-unloaded",
                             G_CALLBACK (subdirectory_unloaded_callback), view, 0);

    g_signal_connect_object (view->details->model, "get-icon-scale",
                             G_CALLBACK (get_icon_scale_callback), view, 0);

    gesture = gtk_gesture_long_press_new (GTK_WIDGET (content_widget));
    view->details->long_press_gesture = gesture;

    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                                GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), TRUE);
    g_signal_connect (gesture,
                      "pressed",
                      (GCallback) on_longpress_gesture_pressed_event,
                      view);

    gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view->details->tree_view), GTK_SELECTION_MULTIPLE);

    g_settings_bind (nautilus_list_view_preferences, NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE,
                     view->details->tree_view, "show-expanders",
                     G_SETTINGS_BIND_DEFAULT);

    nautilus_columns = nautilus_get_all_columns ();

    for (l = nautilus_columns; l != NULL; l = l->next)
    {
        NautilusColumn *nautilus_column;
        int column_num;
        char *name;
        char *label;
        float xalign;
        GtkSortType sort_order;

        nautilus_column = NAUTILUS_COLUMN (l->data);

        g_object_get (nautilus_column,
                      "name", &name,
                      "label", &label,
                      "xalign", &xalign,
                      "default-sort-order", &sort_order,
                      NULL);

        column_num = nautilus_list_model_add_column (view->details->model,
                                                     nautilus_column);

        /* Created the name column specially, because it
         * has the icon in it.*/
        if (!strcmp (name, "name"))
        {
            /* Create the file name column */
            view->details->file_name_column = gtk_tree_view_column_new ();
            gtk_tree_view_append_column (view->details->tree_view,
                                         view->details->file_name_column);
            view->details->file_name_column_num = column_num;

            g_hash_table_insert (view->details->columns,
                                 g_strdup ("name"),
                                 view->details->file_name_column);

            gtk_tree_view_set_search_column (view->details->tree_view, column_num);

            gtk_tree_view_column_set_sort_column_id (view->details->file_name_column, column_num);
            gtk_tree_view_column_set_title (view->details->file_name_column, _("Name"));
            gtk_tree_view_column_set_resizable (view->details->file_name_column, TRUE);
            gtk_tree_view_column_set_expand (view->details->file_name_column, TRUE);

            /* Initial padding */
            cell = gtk_cell_renderer_text_new ();
            gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
            g_object_set (cell, "xpad", 6, NULL);
            g_settings_bind (nautilus_list_view_preferences, NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE,
                             cell, "visible",
                             G_SETTINGS_BIND_INVERT_BOOLEAN | G_SETTINGS_BIND_GET);

            /* File icon */
            cell = gtk_cell_renderer_pixbuf_new ();
            view->details->pixbuf_cell = (GtkCellRendererPixbuf *) cell;
            set_up_pixbuf_size (view);

            gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
            gtk_tree_view_column_set_attributes (view->details->file_name_column,
                                                 cell,
                                                 "surface", nautilus_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
                                                 NULL);

            cell = gtk_cell_renderer_text_new ();
            view->details->file_name_cell = (GtkCellRendererText *) cell;
            g_object_set (cell,
                          "ellipsize", PANGO_ELLIPSIZE_END,
                          "single-paragraph-mode", FALSE,
                          "width-chars", 30,
                          "xpad", 5,
                          NULL);

            gtk_tree_view_column_pack_start (view->details->file_name_column, cell, TRUE);
            gtk_tree_view_column_set_cell_data_func (view->details->file_name_column, cell,
                                                     (GtkTreeCellDataFunc) filename_cell_data_func,
                                                     view, NULL);
        }
        else
        {
            if (g_strcmp0 (name, "starred") == 0)
            {
                cell = gtk_cell_renderer_pixbuf_new ();
                g_object_set (cell,
                              "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                              NULL);

                column = gtk_tree_view_column_new_with_attributes ("",
                                                                   cell,
                                                                   NULL);
                gtk_tree_view_column_set_fixed_width (column, STAR_COLUMN_WIDTH);
            }
            else
            {
                PangoAttrList *attr_list = pango_attr_list_new ();

                cell = gtk_cell_renderer_text_new ();

                column = gtk_tree_view_column_new_with_attributes (label,
                                                                   cell,
                                                                   "text", column_num,
                                                                   NULL);

                pango_attr_list_insert (attr_list, pango_attr_foreground_alpha_new (ALPHA_55_PERCENT));
                g_object_set (cell, "attributes", attr_list, NULL);
                pango_attr_list_unref (attr_list);
            }

            gtk_tree_view_column_set_alignment (column, xalign);
            g_object_set (cell,
                          "xalign", xalign,
                          "xpad", 5,
                          NULL);
            if (!strcmp (name, "permissions"))
            {
                g_object_set (cell,
                              "family", "Monospace",
                              NULL);
            }
            view->details->cells = g_list_append (view->details->cells,
                                                  cell);

            gtk_tree_view_append_column (view->details->tree_view, column);
            gtk_tree_view_column_set_sort_column_id (column, column_num);
            g_hash_table_insert (view->details->columns,
                                 g_strdup (name),
                                 column);

            gtk_tree_view_column_set_resizable (column, TRUE);
            gtk_tree_view_column_set_sort_order (column, sort_order);

            if (!strcmp (name, "where"))
            {
                gtk_tree_view_column_set_cell_data_func (column, cell,
                                                         (GtkTreeCellDataFunc) where_cell_data_func,
                                                         view, NULL);
            }
            else if (!strcmp (name, "trash_orig_path"))
            {
                gtk_tree_view_column_set_cell_data_func (column, cell,
                                                         (GtkTreeCellDataFunc) trash_orig_path_cell_data_func,
                                                         view, NULL);
            }
            else if (!strcmp (name, "starred"))
            {
                gtk_tree_view_column_set_cell_data_func (column, cell,
                                                         (GtkTreeCellDataFunc) starred_cell_data_func,
                                                         view, NULL);
            }
        }
        g_free (name);
        g_free (label);
    }
    nautilus_column_list_free (nautilus_columns);

    default_visible_columns = g_settings_get_strv (nautilus_list_view_preferences,
                                                   NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
    default_column_order = g_settings_get_strv (nautilus_list_view_preferences,
                                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

    /* Apply the default column order and visible columns, to get it
     * right most of the time. The metadata will be checked when a
     * folder is loaded */
    apply_columns_settings (view,
                            default_column_order,
                            default_visible_columns);

    gtk_widget_show (GTK_WIDGET (view->details->tree_view));
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_widget),
                                   GTK_WIDGET (view->details->tree_view));

    atk_obj = gtk_widget_get_accessible (GTK_WIDGET (view->details->tree_view));
    atk_object_set_name (atk_obj, _("List View"));

    g_strfreev (default_visible_columns);
    g_strfreev (default_column_order);
}

static void
nautilus_list_view_add_files (NautilusFilesView *view,
                              GList             *files)
{
    NautilusListModel *model;
    GList *l;

    model = NAUTILUS_LIST_VIEW (view)->details->model;
    for (l = files; l != NULL; l = l->next)
    {
        NautilusFile *parent;
        NautilusDirectory *directory;

        parent = nautilus_file_get_parent (NAUTILUS_FILE (l->data));
        directory = nautilus_directory_get_for_file (parent);
        nautilus_list_model_add_file (model, NAUTILUS_FILE (l->data), directory);

        nautilus_file_unref (parent);
        nautilus_directory_unref (directory);
    }
}

static char **
get_default_visible_columns (NautilusListView *list_view)
{
    NautilusFile *file;
    NautilusDirectory *directory;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));

    if (nautilus_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_trash_visible_columns);
    }

    if (nautilus_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_recent_visible_columns);
    }

    directory = nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (list_view));
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        return g_strdupv ((gchar **) default_search_visible_columns);
    }

    return g_settings_get_strv (nautilus_list_view_preferences,
                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
}

static GList *
get_default_visible_columns_as_list (NautilusListView *list_view)
{
    GList *res = NULL;
    gint i = 0;
    gchar **array;

    array = get_default_visible_columns (list_view);

    while (array[i] != NULL)
    {
        res = g_list_prepend (res, array[i]);
        i++;
    }

    g_free (array);

    return res;
}

static char **
get_visible_columns (NautilusListView *list_view)
{
    NautilusFile *file;
    g_autoptr (GList) visible_columns = NULL;
    g_autoptr (GFile) location = NULL;
    GPtrArray *res;
    GList *l;
    g_autofree gchar *uri = NULL;
    gboolean can_star_current_directory;
    gboolean is_starred;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));
    uri = nautilus_file_get_uri (file);

    location = g_file_new_for_uri (uri);
    can_star_current_directory = nautilus_tag_manager_can_star_contents (list_view->details->tag_manager,
                                                                         location);
    is_starred = eel_uri_is_starred (uri);

    visible_columns = nautilus_file_get_metadata_list (file,
                                                       NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);
    if (visible_columns == NULL)
    {
        visible_columns = get_default_visible_columns_as_list (list_view);
    }

    res = g_ptr_array_new ();
    for (l = visible_columns; l != NULL; l = l->next)
    {
        if (g_strcmp0 (l->data, "starred") != 0 ||
            (g_strcmp0 (l->data, "starred") == 0 && (can_star_current_directory || is_starred)))
        {
            g_ptr_array_add (res, l->data);
        }
    }

    g_ptr_array_add (res, NULL);

    return (char **) g_ptr_array_free (res, FALSE);
}

static char **
get_default_column_order (NautilusListView *list_view)
{
    NautilusFile *file;
    NautilusDirectory *directory;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));

    if (nautilus_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_trash_columns_order);
    }

    if (nautilus_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_recent_columns_order);
    }

    directory = nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (list_view));
    if (NAUTILUS_IS_SEARCH_DIRECTORY (directory))
    {
        return g_strdupv ((gchar **) default_search_columns_order);
    }

    return g_settings_get_strv (nautilus_list_view_preferences,
                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
}

static char **
get_column_order (NautilusListView *list_view)
{
    NautilusFile *file;
    GList *column_order;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));

    column_order = nautilus_file_get_metadata_list
                       (file,
                       NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);

    if (column_order)
    {
        GPtrArray *res;
        GList *l;

        res = g_ptr_array_new ();
        for (l = column_order; l != NULL; l = l->next)
        {
            g_ptr_array_add (res, l->data);
        }
        g_ptr_array_add (res, NULL);

        g_list_free (column_order);

        return (char **) g_ptr_array_free (res, FALSE);
    }

    return get_default_column_order (list_view);
}

static void
check_allow_sort (NautilusListView *list_view)
{
    GList *column_names;
    GList *l;
    NautilusFile *file;
    GtkTreeViewColumn *column;
    gboolean allow_sorting;
    int sort_column_id;

    column_names = g_hash_table_get_keys (list_view->details->columns);
    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));
    allow_sorting = !(nautilus_file_is_in_recent (file) || nautilus_file_is_in_search (file));

    for (l = column_names; l != NULL; l = l->next)
    {
        column = g_hash_table_lookup (list_view->details->columns, l->data);
        if (allow_sorting)
        {
            sort_column_id = nautilus_list_model_get_sort_column_id_from_attribute (list_view->details->model,
                                                                                    g_quark_from_string (l->data));
            /* Restore its original sorting id. We rely on that the keys of the hashmap
             * use the same string than the sort criterias */
            gtk_tree_view_column_set_sort_column_id (column, sort_column_id);
        }
        else
        {
            /* This disables the header and any sorting capability (like shortcuts),
             * but leaving them interactionable so the user can still resize them */
            gtk_tree_view_column_set_sort_column_id (column, -1);
        }
    }

    g_list_free (column_names);
}

static void
set_columns_settings_from_metadata_and_preferences (NautilusListView *list_view)
{
    char **column_order;
    char **visible_columns;

    column_order = get_column_order (list_view);
    visible_columns = get_visible_columns (list_view);

    apply_columns_settings (list_view, column_order, visible_columns);

    g_strfreev (column_order);
    g_strfreev (visible_columns);
}

static void
set_sort_order_from_metadata_and_preferences (NautilusListView *list_view)
{
    char *sort_attribute;
    int sort_column_id;
    NautilusFile *file;
    gboolean sort_reversed, default_sort_reversed;
    const gchar *default_sort_order;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (list_view));
    default_sort_order = get_default_sort_order (file, &default_sort_reversed);
    if (!(nautilus_file_is_in_recent (file) || nautilus_file_is_in_search (file)))
    {
        sort_attribute = nautilus_file_get_metadata (file,
                                                     NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
                                                     NULL);
        sort_column_id = nautilus_list_model_get_sort_column_id_from_attribute (list_view->details->model,
                                                                                g_quark_from_string (sort_attribute));
        g_free (sort_attribute);

        if (sort_column_id == -1)
        {
            sort_column_id =
                nautilus_list_model_get_sort_column_id_from_attribute (list_view->details->model,
                                                                       g_quark_from_string (default_sort_order));
        }

        sort_reversed = nautilus_file_get_boolean_metadata (file,
                                                            NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
                                                            default_sort_reversed);
    }
    else
    {
        /* Make sure we use the default one and not one that the user used previously
         * of the change to not allow sorting on search and recent, or the
         * case that the user or some app modified directly the metadata */
        sort_column_id = nautilus_list_model_get_sort_column_id_from_attribute (list_view->details->model,
                                                                                g_quark_from_string (default_sort_order));
        sort_reversed = default_sort_reversed;
    }
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_view->details->model),
                                          sort_column_id,
                                          sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);
}

static NautilusListZoomLevel
get_default_zoom_level (void)
{
    NautilusListZoomLevel default_zoom_level;

    default_zoom_level = g_settings_get_enum (nautilus_list_view_preferences,
                                              NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL);

    if (default_zoom_level < NAUTILUS_LIST_ZOOM_LEVEL_SMALL
        || default_zoom_level > NAUTILUS_LIST_ZOOM_LEVEL_LARGER)
    {
        default_zoom_level = NAUTILUS_LIST_ZOOM_LEVEL_STANDARD;
    }

    return default_zoom_level;
}

static void
nautilus_list_view_begin_loading (NautilusFilesView *view)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (view);

    nautilus_list_view_sort_directories_first_changed (NAUTILUS_FILES_VIEW (list_view));
    set_sort_order_from_metadata_and_preferences (list_view);
    set_columns_settings_from_metadata_and_preferences (list_view);
    check_allow_sort (list_view);
}

static void
nautilus_list_view_clear (NautilusFilesView *view)
{
    NautilusListView *list_view;
    GtkTreeView *tree_view;
    GtkTreeSelection *tree_selection;
    GtkTreePath *path;

    list_view = NAUTILUS_LIST_VIEW (view);

    if (list_view->details->model != NULL)
    {
        tree_view = list_view->details->tree_view;

        /* When the current cursor's row gets deleted, GTK will move the cursor to
         * the next row, and when setting the cursor it also selects the new
         * cursor's row, thereby triggering selection signals. The new cursor will
         * soon be deleted again and the loop repeats.
         *
         * Since clear() removes all entries, those selections are useless but they
         * take up most of the time in clear(). For example, when a search returns
         * a large list, exiting from the search view would make nautilus hang.
         *
         * At the time the code is written simply removing the cursor solves the
         * problem, but to be future-proof in case GTK does anything fancy with
         * the current selection, we also remove the selection.
         *
         * Because GTK internally seeking the cursor takes time, only blocking the
         * selection signal like everywhere else will not remove that overhead.
         */

        /* Clear the current selection */
        tree_selection = gtk_tree_view_get_selection (tree_view);
        gtk_tree_selection_unselect_all (tree_selection);

        /* Clear the current cursor */
        path = gtk_tree_path_new ();
        gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
        gtk_tree_path_free (path);

        nautilus_list_model_clear (list_view->details->model);
    }
}

static void
nautilus_list_view_file_changed (NautilusFilesView *view,
                                 NautilusFile      *file,
                                 NautilusDirectory *directory)
{
    NautilusListView *listview;

    listview = NAUTILUS_LIST_VIEW (view);

    nautilus_list_model_file_changed (listview->details->model, file, directory);
}

typedef struct
{
    GtkTreePath *path;
    gboolean is_common;
    gboolean is_root;
} HasCommonParentData;

static void
tree_selection_has_common_parent_foreach_func (GtkTreeModel *model,
                                               GtkTreePath  *path,
                                               GtkTreeIter  *iter,
                                               gpointer      user_data)
{
    HasCommonParentData *data;
    GtkTreePath *parent_path;
    gboolean has_parent;

    data = (HasCommonParentData *) user_data;

    parent_path = gtk_tree_path_copy (path);
    gtk_tree_path_up (parent_path);

    has_parent = (gtk_tree_path_get_depth (parent_path) > 0) ? TRUE : FALSE;

    if (!has_parent)
    {
        data->is_root = TRUE;
    }

    if (data->is_common && !data->is_root)
    {
        if (data->path == NULL)
        {
            data->path = gtk_tree_path_copy (parent_path);
        }
        else if (gtk_tree_path_compare (data->path, parent_path) != 0)
        {
            data->is_common = FALSE;
        }
    }

    gtk_tree_path_free (parent_path);
}

static void
tree_selection_has_common_parent (GtkTreeSelection *selection,
                                  gboolean         *is_common,
                                  gboolean         *is_root)
{
    HasCommonParentData data;

    g_assert (is_common != NULL);
    g_assert (is_root != NULL);

    data.path = NULL;
    data.is_common = *is_common = TRUE;
    data.is_root = *is_root = FALSE;

    gtk_tree_selection_selected_foreach (selection,
                                         tree_selection_has_common_parent_foreach_func,
                                         &data);

    *is_common = data.is_common;
    *is_root = data.is_root;

    if (data.path != NULL)
    {
        gtk_tree_path_free (data.path);
    }
}

static char *
nautilus_list_view_get_backing_uri (NautilusFilesView *view)
{
    NautilusListView *list_view;
    NautilusListModel *list_model;
    NautilusFile *file;
    GtkTreeView *tree_view;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    GList *paths;
    guint length;
    char *uri;

    g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), NULL);

    list_view = NAUTILUS_LIST_VIEW (view);
    list_model = list_view->details->model;
    tree_view = list_view->details->tree_view;

    g_assert (list_model);

    /* We currently handle three common cases here:
     * (a) if the selection contains non-filesystem items (i.e., the
     *     "(Empty)" label), we return the uri of the parent.
     * (b) if the selection consists of exactly one _expanded_ directory, we
     *     return its URI.
     * (c) if the selection consists of either exactly one item which is not
     *     an expanded directory) or multiple items in the same directory,
     *     we return the URI of the common parent.
     */

    uri = NULL;

    selection = gtk_tree_view_get_selection (tree_view);
    length = gtk_tree_selection_count_selected_rows (selection);

    if (length == 1)
    {
        paths = gtk_tree_selection_get_selected_rows (selection, NULL);
        path = (GtkTreePath *) paths->data;

        file = nautilus_list_model_file_for_path (list_model, path);
        if (file == NULL)
        {
            /* The selected item is a label, not a file */
            gtk_tree_path_up (path);
            file = nautilus_list_model_file_for_path (list_model, path);
        }

        if (file != NULL)
        {
            if (nautilus_file_is_directory (file) &&
                gtk_tree_view_row_expanded (tree_view, path))
            {
                uri = nautilus_file_get_uri (file);
            }
            nautilus_file_unref (file);
        }

        gtk_tree_path_free (path);
        g_list_free (paths);
    }

    if (uri == NULL && length > 0)
    {
        gboolean is_common, is_root;

        /* Check that all the selected items belong to the same
         * directory and that directory is not the root directory (which
         * is handled by NautilusFilesView::get_backing_directory.) */

        tree_selection_has_common_parent (selection, &is_common, &is_root);

        if (is_common && !is_root)
        {
            paths = gtk_tree_selection_get_selected_rows (selection, NULL);
            path = (GtkTreePath *) paths->data;

            file = nautilus_list_model_file_for_path (list_model, path);
            g_assert (file != NULL);
            uri = nautilus_file_get_parent_uri (file);
            nautilus_file_unref (file);

            g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
        }
    }

    if (uri != NULL)
    {
        return uri;
    }

    return NAUTILUS_FILES_VIEW_CLASS (nautilus_list_view_parent_class)->get_backing_uri (view);
}

static void
nautilus_list_view_get_selection_foreach_func (GtkTreeModel *model,
                                               GtkTreePath  *path,
                                               GtkTreeIter  *iter,
                                               gpointer      data)
{
    GList **list;
    NautilusFile *file;

    list = data;

    gtk_tree_model_get (model, iter,
                        NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    if (file != NULL)
    {
        (*list) = g_list_prepend ((*list), file);
    }
}

static GList *
nautilus_list_view_get_selection (NautilusFilesView *view)
{
    GList *list;

    list = NULL;

    gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view),
                                         nautilus_list_view_get_selection_foreach_func, &list);

    return g_list_reverse (list);
}

static void
nautilus_list_view_get_selection_for_file_transfer_foreach_func (GtkTreeModel *model,
                                                                 GtkTreePath  *path,
                                                                 GtkTreeIter  *iter,
                                                                 gpointer      data)
{
    NautilusFile *file;
    struct SelectionForeachData *selection_data;
    GtkTreeIter parent, child;

    selection_data = data;

    gtk_tree_model_get (model, iter,
                        NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                        -1);

    if (file != NULL)
    {
        /* If the parent folder is also selected, don't include this file in the
         * file operation, since that would copy it to the toplevel target instead
         * of keeping it as a child of the copied folder
         */
        child = *iter;
        while (gtk_tree_model_iter_parent (model, &parent, &child))
        {
            if (gtk_tree_selection_iter_is_selected (selection_data->selection,
                                                     &parent))
            {
                return;
            }
            child = parent;
        }

        nautilus_file_ref (file);
        selection_data->list = g_list_prepend (selection_data->list, file);
    }
}


static GList *
nautilus_list_view_get_selection_for_file_transfer (NautilusFilesView *view)
{
    struct SelectionForeachData selection_data;

    selection_data.list = NULL;
    selection_data.selection = gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view);

    gtk_tree_selection_selected_foreach (selection_data.selection,
                                         nautilus_list_view_get_selection_for_file_transfer_foreach_func, &selection_data);

    return g_list_reverse (selection_data.list);
}

static gboolean
nautilus_list_view_is_empty (NautilusFilesView *view)
{
    return nautilus_list_model_is_empty (NAUTILUS_LIST_VIEW (view)->details->model);
}

static void
nautilus_list_view_end_file_changes (NautilusFilesView *view)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (view);

    if (list_view->details->new_selection_path)
    {
        gtk_tree_view_set_cursor (list_view->details->tree_view,
                                  list_view->details->new_selection_path,
                                  NULL, FALSE);
        gtk_tree_path_free (list_view->details->new_selection_path);
        list_view->details->new_selection_path = NULL;
    }
}

static void
nautilus_list_view_remove_file (NautilusFilesView *view,
                                NautilusFile      *file,
                                NautilusDirectory *directory)
{
    GtkTreePath *path;
    GtkTreePath *file_path;
    GtkTreeIter iter;
    GtkTreeIter temp_iter;
    GtkTreeRowReference *row_reference;
    NautilusListView *list_view;
    GtkTreeModel *tree_model;
    GtkTreeSelection *selection;

    path = NULL;
    row_reference = NULL;
    list_view = NAUTILUS_LIST_VIEW (view);
    tree_model = GTK_TREE_MODEL (list_view->details->model);

    if (nautilus_list_model_get_tree_iter_from_file (list_view->details->model, file, directory, &iter))
    {
        selection = gtk_tree_view_get_selection (list_view->details->tree_view);
        file_path = gtk_tree_model_get_path (tree_model, &iter);

        if (gtk_tree_selection_path_is_selected (selection, file_path))
        {
            /* get reference for next element in the list view. If the element to be deleted is the
             * last one, get reference to previous element. If there is only one element in view
             * no need to select anything.
             */
            temp_iter = iter;

            if (gtk_tree_model_iter_next (tree_model, &iter))
            {
                path = gtk_tree_model_get_path (tree_model, &iter);
                row_reference = gtk_tree_row_reference_new (tree_model, path);
            }
            else
            {
                path = gtk_tree_model_get_path (tree_model, &temp_iter);
                if (gtk_tree_path_prev (path))
                {
                    row_reference = gtk_tree_row_reference_new (tree_model, path);
                }
            }
            gtk_tree_path_free (path);
        }

        gtk_tree_path_free (file_path);

        nautilus_list_model_remove_file (list_view->details->model, file, directory);

        if (gtk_tree_row_reference_valid (row_reference))
        {
            if (list_view->details->new_selection_path)
            {
                gtk_tree_path_free (list_view->details->new_selection_path);
            }
            list_view->details->new_selection_path = gtk_tree_row_reference_get_path (row_reference);
        }

        if (row_reference)
        {
            gtk_tree_row_reference_free (row_reference);
        }
    }
}

static void
nautilus_list_view_set_selection (NautilusFilesView *view,
                                  GList             *selection)
{
    NautilusListView *list_view;
    NautilusListModel *model;
    GtkTreeView *tree_view;
    GtkTreeSelection *tree_selection;
    GList *node;
    gboolean cursor_is_set_on_selection = FALSE;
    GList *iters, *l;
    NautilusFile *file;

    list_view = NAUTILUS_LIST_VIEW (view);
    model = list_view->details->model;
    tree_view = list_view->details->tree_view;
    tree_selection = gtk_tree_view_get_selection (tree_view);

    g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

    gtk_tree_selection_unselect_all (tree_selection);
    for (node = selection; node != NULL; node = node->next)
    {
        file = node->data;
        iters = nautilus_list_model_get_all_iters_for_file (model, file);

        for (l = iters; l != NULL; l = l->next)
        {
            if (!cursor_is_set_on_selection)
            {
                GtkTreePath *path;

                path = gtk_tree_model_get_path (GTK_TREE_MODEL (model),
                                                (GtkTreeIter *) l->data);
                gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
                gtk_tree_path_free (path);

                cursor_is_set_on_selection = TRUE;
                continue;
            }

            gtk_tree_selection_select_iter (tree_selection,
                                            (GtkTreeIter *) l->data);
        }
        g_list_free_full (iters, g_free);
    }

    g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
    nautilus_files_view_notify_selection_changed (view);
}

static void
nautilus_list_view_invert_selection (NautilusFilesView *view)
{
    NautilusListView *list_view;
    GtkTreeSelection *tree_selection;
    GList *node;
    GList *iters, *l;
    NautilusFile *file;
    GList *selection = NULL;

    list_view = NAUTILUS_LIST_VIEW (view);
    tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

    g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

    gtk_tree_selection_selected_foreach (tree_selection,
                                         nautilus_list_view_get_selection_foreach_func, &selection);

    gtk_tree_selection_select_all (tree_selection);

    for (node = selection; node != NULL; node = node->next)
    {
        file = node->data;
        iters = nautilus_list_model_get_all_iters_for_file (list_view->details->model, file);

        for (l = iters; l != NULL; l = l->next)
        {
            gtk_tree_selection_unselect_iter (tree_selection,
                                              (GtkTreeIter *) l->data);
        }
        g_list_free_full (iters, g_free);
    }

    g_list_free (selection);

    g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
    nautilus_files_view_notify_selection_changed (view);
}

static void
nautilus_list_view_select_all (NautilusFilesView *view)
{
    gtk_tree_selection_select_all (gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view));
}

static void
nautilus_list_view_select_first (NautilusFilesView *view)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (NAUTILUS_LIST_VIEW (view)->details->model), &iter))
    {
        return;
    }
    selection = gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view);
    gtk_tree_selection_unselect_all (selection);
    gtk_tree_selection_select_iter (selection, &iter);
}

static void
nautilus_list_view_zoom_to_level (NautilusFilesView *view,
                                  gint               zoom_level)
{
    NautilusListView *list_view;

    g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

    list_view = NAUTILUS_LIST_VIEW (view);

    if (list_view->details->zoom_level == zoom_level)
    {
        return;
    }

    nautilus_list_view_set_zoom_level (list_view, zoom_level);
    g_action_group_change_action_state (nautilus_files_view_get_action_group (view),
                                        "zoom-to-level", g_variant_new_int32 (zoom_level));

    nautilus_files_view_update_toolbar_menus (view);
}

static void
action_zoom_to_level (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusFilesView *view;
    NautilusListZoomLevel zoom_level;

    g_assert (NAUTILUS_IS_FILES_VIEW (user_data));

    view = NAUTILUS_FILES_VIEW (user_data);
    zoom_level = g_variant_get_int32 (state);
    nautilus_list_view_zoom_to_level (view, zoom_level);

    g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
    if (g_settings_get_enum (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL) != zoom_level)
    {
        g_settings_set_enum (nautilus_list_view_preferences,
                             NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
                             zoom_level);
    }
}

static void
column_chooser_changed_callback (NautilusColumnChooser *chooser,
                                 NautilusListView      *view)
{
    NautilusFile *file;
    char **visible_columns;
    char **column_order;
    GList *list;
    int i;

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (view));

    nautilus_column_chooser_get_settings (chooser,
                                          &visible_columns,
                                          &column_order);

    list = NULL;
    for (i = 0; visible_columns[i] != NULL; ++i)
    {
        list = g_list_prepend (list, visible_columns[i]);
    }
    list = g_list_reverse (list);
    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                     list);
    g_list_free (list);

    list = NULL;
    for (i = 0; column_order[i] != NULL; ++i)
    {
        list = g_list_prepend (list, column_order[i]);
    }
    list = g_list_reverse (list);
    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
                                     list);
    g_list_free (list);

    apply_columns_settings (view, column_order, visible_columns);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
column_chooser_set_from_arrays (NautilusColumnChooser  *chooser,
                                NautilusListView       *view,
                                char                  **visible_columns,
                                char                  **column_order)
{
    g_signal_handlers_block_by_func
        (chooser, G_CALLBACK (column_chooser_changed_callback), view);

    nautilus_column_chooser_set_settings (chooser,
                                          visible_columns,
                                          column_order);

    g_signal_handlers_unblock_by_func
        (chooser, G_CALLBACK (column_chooser_changed_callback), view);
}

static void
column_chooser_set_from_settings (NautilusColumnChooser *chooser,
                                  NautilusListView      *view)
{
    char **visible_columns;
    char **column_order;

    visible_columns = get_visible_columns (view);
    column_order = get_column_order (view);

    column_chooser_set_from_arrays (chooser, view,
                                    visible_columns, column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
column_chooser_use_default_callback (NautilusColumnChooser *chooser,
                                     NautilusListView      *view)
{
    NautilusFile *file;
    char **default_columns;
    char **default_order;

    file = nautilus_files_view_get_directory_as_file
               (NAUTILUS_FILES_VIEW (view));

    nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
    nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

    /* set view values ourselves, as new metadata could not have been
     * updated yet.
     */
    default_columns = get_default_visible_columns (view);
    default_order = get_default_column_order (view);

    apply_columns_settings (view, default_order, default_columns);
    column_chooser_set_from_arrays (chooser, view,
                                    default_columns, default_order);

    g_strfreev (default_columns);
    g_strfreev (default_order);
}

static GtkWidget *
create_column_editor (NautilusListView *view)
{
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *column_chooser;
    NautilusFile *file;
    char *str;
    char *name;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-list-view-column-editor.ui");

    window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
    gtk_window_set_transient_for (GTK_WINDOW (window),
                                  GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))));

    file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (view));
    name = nautilus_file_get_display_name (file);
    str = g_strdup_printf (_("%s Visible Columns"), name);
    g_free (name);
    gtk_window_set_title (GTK_WINDOW (window), str);
    g_free (str);

    box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));

    column_chooser = nautilus_column_chooser_new (file);
    gtk_widget_set_vexpand (column_chooser, TRUE);
    gtk_box_append (GTK_BOX (box), column_chooser);

    g_signal_connect (column_chooser, "changed",
                      G_CALLBACK (column_chooser_changed_callback),
                      view);
    g_signal_connect (column_chooser, "use-default",
                      G_CALLBACK (column_chooser_use_default_callback),
                      view);

    column_chooser_set_from_settings
        (NAUTILUS_COLUMN_CHOOSER (column_chooser), view);

    return window;
}

static void
action_visible_columns (GSimpleAction *action,
                        GVariant      *state,
                        gpointer       user_data)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (user_data);

    if (list_view->details->column_editor)
    {
        gtk_window_present (GTK_WINDOW (list_view->details->column_editor));
    }
    else
    {
        list_view->details->column_editor = create_column_editor (list_view);
        g_object_add_weak_pointer (G_OBJECT (list_view->details->column_editor),
                                   (gpointer *) &list_view->details->column_editor);

        gtk_widget_show (list_view->details->column_editor);
    }
}

const GActionEntry list_view_entries[] =
{
    { "visible-columns", action_visible_columns },
    { "zoom-to-level", NULL, NULL, "1", action_zoom_to_level }
};

static void
nautilus_list_view_set_zoom_level (NautilusListView      *view,
                                   NautilusListZoomLevel  new_level)
{
    int column;

    g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));
    g_return_if_fail (new_level >= NAUTILUS_LIST_ZOOM_LEVEL_SMALL &&
                      new_level <= NAUTILUS_LIST_ZOOM_LEVEL_LARGER);

    if (view->details->zoom_level == new_level)
    {
        return;
    }

    view->details->zoom_level = new_level;

    /* Select correctly scaled icons. */
    column = nautilus_list_model_get_column_id_from_zoom_level (new_level);
    gtk_tree_view_column_set_attributes (view->details->file_name_column,
                                         GTK_CELL_RENDERER (view->details->pixbuf_cell),
                                         "surface", column,
                                         NULL);
    set_up_pixbuf_size (view);
}

static void
nautilus_list_view_bump_zoom_level (NautilusFilesView *view,
                                    int                zoom_increment)
{
    NautilusListView *list_view;
    gint new_level;

    g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

    list_view = NAUTILUS_LIST_VIEW (view);
    new_level = list_view->details->zoom_level + zoom_increment;

    if (new_level >= NAUTILUS_LIST_ZOOM_LEVEL_SMALL &&
        new_level <= NAUTILUS_LIST_ZOOM_LEVEL_LARGER)
    {
        nautilus_list_view_zoom_to_level (view, new_level);
    }
}

static void
nautilus_list_view_restore_standard_zoom_level (NautilusFilesView *view)
{
    nautilus_list_view_zoom_to_level (view, NAUTILUS_LIST_ZOOM_LEVEL_STANDARD);
}

static gboolean
nautilus_list_view_can_zoom_in (NautilusFilesView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), FALSE);

    return NAUTILUS_LIST_VIEW (view)->details->zoom_level < NAUTILUS_LIST_ZOOM_LEVEL_LARGER;
}

static gboolean
nautilus_list_view_can_zoom_out (NautilusFilesView *view)
{
    g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), FALSE);

    return NAUTILUS_LIST_VIEW (view)->details->zoom_level > NAUTILUS_LIST_ZOOM_LEVEL_SMALL;
}

static gfloat
nautilus_list_view_get_zoom_level_percentage (NautilusFilesView *view)
{
    NautilusListView *list_view;
    guint icon_size;

    g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), 1.0);

    list_view = NAUTILUS_LIST_VIEW (view);
    icon_size = nautilus_list_model_get_icon_size_for_zoom_level (list_view->details->zoom_level);

    return (gfloat) icon_size / NAUTILUS_LIST_ICON_SIZE_STANDARD;
}

static gboolean
nautilus_list_view_is_zoom_level_default (NautilusFilesView *view)
{
    NautilusListView *list_view;
    guint icon_size;

    list_view = NAUTILUS_LIST_VIEW (view);
    icon_size = nautilus_list_model_get_icon_size_for_zoom_level (list_view->details->zoom_level);

    return icon_size == NAUTILUS_LIST_ICON_SIZE_STANDARD;
}

static void
nautilus_list_view_click_policy_changed (NautilusFilesView *directory_view)
{
    GdkWindow *win;
    GdkDisplay *display;
    NautilusListView *view;
    GtkTreeIter iter;
    GtkTreeView *tree;

    view = NAUTILUS_LIST_VIEW (directory_view);
    display = gtk_widget_get_display (GTK_WIDGET (view));

    /* ensure that we unset the hand cursor and refresh underlined rows */
    if (get_click_policy () == NAUTILUS_CLICK_POLICY_DOUBLE)
    {
        if (view->details->hover_path != NULL)
        {
            if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
                                         &iter, view->details->hover_path))
            {
                gtk_tree_model_row_changed (GTK_TREE_MODEL (view->details->model),
                                            view->details->hover_path, &iter);
            }

            gtk_tree_path_free (view->details->hover_path);
            view->details->hover_path = NULL;
        }

        tree = view->details->tree_view;
        if (gtk_widget_get_realized (GTK_WIDGET (tree)))
        {
            win = gtk_widget_get_window (GTK_WIDGET (tree));
            gdk_window_set_cursor (win, NULL);

            if (display != NULL)
            {
                gdk_display_flush (display);
            }
        }

        g_clear_object (&hand_cursor);
    }
    else if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE)
    {
        if (hand_cursor == NULL)
        {
            hand_cursor = gdk_cursor_new_from_name (display, "pointer");
        }
    }
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (callback_data);

    set_sort_order_from_metadata_and_preferences (list_view);
}

static void
default_visible_columns_changed_callback (gpointer callback_data)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (callback_data);

    set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
default_column_order_changed_callback (gpointer callback_data)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (callback_data);

    set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
nautilus_list_view_sort_directories_first_changed (NautilusFilesView *view)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (view);

    nautilus_list_model_set_should_sort_directories_first (list_view->details->model,
                                                           nautilus_files_view_should_sort_directories_first (view));
}

static int
nautilus_list_view_compare_files (NautilusFilesView *view,
                                  NautilusFile      *file1,
                                  NautilusFile      *file2)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (view);
    return nautilus_list_model_compare_func (list_view->details->model, file1, file2);
}

static void
nautilus_list_view_dispose (GObject *object)
{
    NautilusListView *list_view;
    GtkClipboard *clipboard;

    list_view = NAUTILUS_LIST_VIEW (object);

    if (list_view->details->model)
    {
        g_object_unref (list_view->details->model);
        list_view->details->model = NULL;
    }

    if (list_view->details->drag_dest)
    {
        g_object_unref (list_view->details->drag_dest);
        list_view->details->drag_dest = NULL;
    }

    clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    g_signal_handlers_disconnect_by_func (clipboard, on_clipboard_owner_changed, list_view);
    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          default_sort_order_changed_callback,
                                          list_view);
    g_signal_handlers_disconnect_by_func (nautilus_list_view_preferences,
                                          default_visible_columns_changed_callback,
                                          list_view);
    g_signal_handlers_disconnect_by_func (nautilus_list_view_preferences,
                                          default_column_order_changed_callback,
                                          list_view);

    g_clear_object (&list_view->details->tree_view_drag_gesture);
    g_clear_object (&list_view->details->tree_view_multi_press_gesture);
    g_clear_object (&list_view->details->motion_controller);
    g_clear_object (&list_view->details->key_controller);
    g_clear_object (&list_view->details->long_press_gesture);

    G_OBJECT_CLASS (nautilus_list_view_parent_class)->dispose (object);
}

static void
nautilus_list_view_finalize (GObject *object)
{
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (object);

    g_free (list_view->details->original_name);
    list_view->details->original_name = NULL;

    if (list_view->details->first_click_path)
    {
        gtk_tree_path_free (list_view->details->first_click_path);
    }
    if (list_view->details->new_selection_path)
    {
        gtk_tree_path_free (list_view->details->new_selection_path);
    }

    g_list_free (list_view->details->cells);
    g_hash_table_destroy (list_view->details->columns);

    if (list_view->details->hover_path != NULL)
    {
        gtk_tree_path_free (list_view->details->hover_path);
    }

    if (list_view->details->column_editor != NULL)
    {
        gtk_widget_destroy (list_view->details->column_editor);
    }

    g_regex_unref (list_view->details->regex);

    g_cancellable_cancel (list_view->details->starred_cancellable);
    g_clear_object (&list_view->details->starred_cancellable);

    g_signal_handlers_disconnect_by_func (list_view->details->tag_manager,
                                          on_starred_files_changed,
                                          list_view);
    g_clear_object (&list_view->details->tag_manager);

    g_free (list_view->details);

    G_OBJECT_CLASS (nautilus_list_view_parent_class)->finalize (object);
}

static char *
nautilus_list_view_get_first_visible_file (NautilusFilesView *view)
{
    NautilusFile *file;
    GtkTreePath *path;
    GtkTreeIter iter;
    NautilusListView *list_view;

    list_view = NAUTILUS_LIST_VIEW (view);

    if (gtk_tree_view_get_path_at_pos (list_view->details->tree_view,
                                       0, 0,
                                       &path, NULL, NULL, NULL))
    {
        gtk_tree_model_get_iter (GTK_TREE_MODEL (list_view->details->model),
                                 &iter, path);

        gtk_tree_path_free (path);

        gtk_tree_model_get (GTK_TREE_MODEL (list_view->details->model),
                            &iter,
                            NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
                            -1);
        if (file)
        {
            char *uri;

            uri = nautilus_file_get_uri (file);

            nautilus_file_unref (file);

            return uri;
        }
    }

    return NULL;
}

static void
nautilus_list_view_scroll_to_file (NautilusListView *view,
                                   NautilusFile     *file)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    if (!nautilus_list_model_get_first_iter_for_file (view->details->model, file, &iter))
    {
        return;
    }

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);

    gtk_tree_view_scroll_to_cell (view->details->tree_view,
                                  path, NULL,
                                  TRUE, 0.0, 0.0);

    gtk_tree_path_free (path);
}

static void
list_view_scroll_to_file (NautilusFilesView *view,
                          const char        *uri)
{
    NautilusFile *file;

    if (uri != NULL)
    {
        /* Only if existing, since we don't want to add the file to
         *  the directory if it has been removed since then */
        file = nautilus_file_get_existing_by_uri (uri);
        if (file != NULL)
        {
            nautilus_list_view_scroll_to_file (NAUTILUS_LIST_VIEW (view), file);
            nautilus_file_unref (file);
        }
    }
}

static void
on_clipboard_contents_received (GtkClipboard     *clipboard,
                                GtkSelectionData *selection_data,
                                gpointer          user_data)
{
    NautilusListView *view = NAUTILUS_LIST_VIEW (user_data);

    if (!view->details->model)
    {
        /* We've been destroyed since call */
        g_object_unref (view);
        return;
    }

    if (nautilus_clipboard_is_cut_from_selection_data (selection_data))
    {
        GList *uris;
        GList *files;

        uris = nautilus_clipboard_get_uri_list_from_selection_data (selection_data);
        files = nautilus_file_list_from_uri_list (uris);
        nautilus_list_model_set_highlight_for_files (view->details->model, files);

        nautilus_file_list_free (files);
        g_list_free_full (uris, g_free);
    }
    else
    {
        nautilus_list_model_set_highlight_for_files (view->details->model, NULL);
    }

    g_object_unref (view);
}

static void
update_clipboard_status (NautilusListView *view)
{
    g_object_ref (view);     /* Need to keep the object alive until we get the reply */
    gtk_clipboard_request_contents (nautilus_clipboard_get (GTK_WIDGET (view)),
                                    nautilus_clipboard_get_atom (),
                                    on_clipboard_contents_received,
                                    view);
}

static void
on_clipboard_owner_changed (GtkClipboard *clipboard,
                            GdkEvent     *event,
                            gpointer      user_data)
{
    update_clipboard_status (NAUTILUS_LIST_VIEW (user_data));
}

static void
nautilus_list_view_end_loading (NautilusFilesView *view,
                                gboolean           all_files_seen)
{
    update_clipboard_status (NAUTILUS_LIST_VIEW (view));
}

static guint
nautilus_list_view_get_id (NautilusFilesView *view)
{
    return NAUTILUS_VIEW_LIST_ID;
}

static GdkRectangle *
get_rectangle_for_path (NautilusListView *list_view,
                        GtkTreePath      *path)
{
    GtkTreeView *tree_view = list_view->details->tree_view;
    GdkRectangle *rect = g_malloc0 (sizeof (GdkRectangle));
    int header_height;

    gtk_tree_view_get_cell_area (tree_view,
                                 path,
                                 list_view->details->file_name_column,
                                 rect);
    gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
                                                       rect->x, rect->y,
                                                       &rect->x, &rect->y);

    /* FIXME Due to smooth scrolling, we may get the cell area while the view is
     * still scrolling (and still outside the view), not at the final position
     * of the cell after scrolling.
     * https://bugzilla.gnome.org/show_bug.cgi?id=746773
     * The following workaround guesses the final "y" coordinate by clamping it
     * to the widget edge. Note that the top edge has got columns header, which
     * is private, so first guess the header height from the difference between
     * widget coordinates and bin cooridinates.
     */
    gtk_tree_view_convert_bin_window_to_widget_coords (tree_view,
                                                       0, 0,
                                                       NULL, &header_height);

    rect->y = CLAMP (rect->y,
                     header_height,
                     gtk_widget_get_allocated_height (GTK_WIDGET (list_view)) - rect->height);
    /* End of workaround */

    return rect;
}

static GdkRectangle *
nautilus_list_view_compute_rename_popover_pointing_to (NautilusFilesView *view)
{
    NautilusListView *list_view;
    GtkTreeView *tree_view;
    GtkTreeSelection *selection;
    GList *list;
    GtkTreePath *path;
    GdkRectangle *rect;

    list_view = NAUTILUS_LIST_VIEW (view);
    tree_view = list_view->details->tree_view;
    selection = gtk_tree_view_get_selection (tree_view);
    list = gtk_tree_selection_get_selected_rows (selection, NULL);
    path = list->data;
    rect = get_rectangle_for_path (list_view, path);

    if (list_view->details->last_event_button_x > 0)
    {
        /* Point to the position in the row where it was clicked. */
        rect->x = list_view->details->last_event_button_x;
        /* Make it zero width to point exactly at rect->x.*/
        rect->width = 0;
    }

    g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

    return rect;
}

static GdkRectangle *
nautilus_list_view_reveal_for_selection_context_menu (NautilusFilesView *view)
{
    NautilusListView *list_view;
    GtkTreeView *tree_view;
    GtkTreeSelection *tree_selection;
    GtkTreePath *path;
    GdkRectangle *rect;

    g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), NULL);

    list_view = NAUTILUS_LIST_VIEW (view);
    tree_view = list_view->details->tree_view;
    tree_selection = gtk_tree_view_get_selection (tree_view);
    g_return_val_if_fail (tree_selection != NULL, NULL);

    /* Get the path to the last focused item, if selected. Otherwise, get
     * the path to the selected item which is sorted the lowest.
     */
    gtk_tree_view_get_cursor (tree_view, &path, NULL);
    if (path == NULL || !gtk_tree_selection_path_is_selected (tree_selection, path))
    {
        GList *list;

        list = gtk_tree_selection_get_selected_rows (tree_selection, NULL);
        list = g_list_last (list);
        path = g_steal_pointer (&list->data);

        g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
    }

    gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);

    rect = get_rectangle_for_path (list_view, path);
    rect->width = rect->height;

    gtk_tree_path_free (path);

    return rect;
}

static void
nautilus_list_view_preview_selection_event (NautilusFilesView *view,
                                            GtkDirectionType   direction)
{
    NautilusListView *list_view;
    GtkTreeView *tree_view;
    GtkTreeSelection *selection;
    GList *list;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *tree_model;
    gboolean moved;

    /* We only support up and down movements for the list view */
    if (direction != GTK_DIR_UP && direction != GTK_DIR_DOWN)
    {
        return;
    }

    list_view = NAUTILUS_LIST_VIEW (view);
    tree_view = list_view->details->tree_view;
    selection = gtk_tree_view_get_selection (tree_view);
    list = gtk_tree_selection_get_selected_rows (selection, &tree_model);

    if (list == NULL)
    {
        return;
    }

    /* Advance the first selected item, since that's what we use for
     * the previewer */
    path = list->data;
    moved = FALSE;
    if (gtk_tree_model_get_iter (tree_model, &iter, path))
    {
        if (direction == GTK_DIR_UP)
        {
            moved = gtk_tree_model_iter_previous (tree_model, &iter);
        }
        else
        {
            moved = gtk_tree_model_iter_next (tree_model, &iter);
        }
    }

    if (moved)
    {
        g_signal_handlers_block_by_func (selection, list_selection_changed_callback, view);

        gtk_tree_selection_unselect_all (selection);
        gtk_tree_selection_select_iter (selection, &iter);

        g_signal_handlers_unblock_by_func (selection, list_selection_changed_callback, view);
        nautilus_files_view_notify_selection_changed (view);
    }

    g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
}

static void
nautilus_list_view_class_init (NautilusListViewClass *class)
{
    NautilusFilesViewClass *nautilus_files_view_class;

    nautilus_files_view_class = NAUTILUS_FILES_VIEW_CLASS (class);

    G_OBJECT_CLASS (class)->dispose = nautilus_list_view_dispose;
    G_OBJECT_CLASS (class)->finalize = nautilus_list_view_finalize;

    nautilus_files_view_class->add_files = nautilus_list_view_add_files;
    nautilus_files_view_class->begin_loading = nautilus_list_view_begin_loading;
    nautilus_files_view_class->end_loading = nautilus_list_view_end_loading;
    nautilus_files_view_class->bump_zoom_level = nautilus_list_view_bump_zoom_level;
    nautilus_files_view_class->can_zoom_in = nautilus_list_view_can_zoom_in;
    nautilus_files_view_class->can_zoom_out = nautilus_list_view_can_zoom_out;
    nautilus_files_view_class->get_zoom_level_percentage = nautilus_list_view_get_zoom_level_percentage;
    nautilus_files_view_class->is_zoom_level_default = nautilus_list_view_is_zoom_level_default;
    nautilus_files_view_class->click_policy_changed = nautilus_list_view_click_policy_changed;
    nautilus_files_view_class->clear = nautilus_list_view_clear;
    nautilus_files_view_class->file_changed = nautilus_list_view_file_changed;
    nautilus_files_view_class->get_backing_uri = nautilus_list_view_get_backing_uri;
    nautilus_files_view_class->get_selection = nautilus_list_view_get_selection;
    nautilus_files_view_class->get_selection_for_file_transfer = nautilus_list_view_get_selection_for_file_transfer;
    nautilus_files_view_class->is_empty = nautilus_list_view_is_empty;
    nautilus_files_view_class->remove_file = nautilus_list_view_remove_file;
    nautilus_files_view_class->restore_standard_zoom_level = nautilus_list_view_restore_standard_zoom_level;
    nautilus_files_view_class->reveal_selection = nautilus_list_view_reveal_selection;
    nautilus_files_view_class->select_all = nautilus_list_view_select_all;
    nautilus_files_view_class->select_first = nautilus_list_view_select_first;
    nautilus_files_view_class->set_selection = nautilus_list_view_set_selection;
    nautilus_files_view_class->invert_selection = nautilus_list_view_invert_selection;
    nautilus_files_view_class->compare_files = nautilus_list_view_compare_files;
    nautilus_files_view_class->sort_directories_first_changed = nautilus_list_view_sort_directories_first_changed;
    nautilus_files_view_class->end_file_changes = nautilus_list_view_end_file_changes;
    nautilus_files_view_class->get_view_id = nautilus_list_view_get_id;
    nautilus_files_view_class->get_first_visible_file = nautilus_list_view_get_first_visible_file;
    nautilus_files_view_class->scroll_to_file = list_view_scroll_to_file;
    nautilus_files_view_class->compute_rename_popover_pointing_to = nautilus_list_view_compute_rename_popover_pointing_to;
    nautilus_files_view_class->reveal_for_selection_context_menu = nautilus_list_view_reveal_for_selection_context_menu;
    nautilus_files_view_class->preview_selection_event = nautilus_list_view_preview_selection_event;
}

static void
nautilus_list_view_init (NautilusListView *list_view)
{
    GActionGroup *view_action_group;
    GtkClipboard *clipboard;

    list_view->details = g_new0 (NautilusListViewDetails, 1);

    /* ensure that the zoom level is always set before settings up the tree view columns */
    list_view->details->zoom_level = get_default_zoom_level ();

    create_and_set_up_tree_view (list_view);

    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (list_view)),
                                 "nautilus-list-view");

    list_view->details->columns_popover = gtk_popover_new (GTK_WIDGET (list_view));
    list_view->details->columns_popover_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top (list_view->details->columns_popover_box, 6);
    gtk_widget_set_margin_bottom (list_view->details->columns_popover_box, 6);
    gtk_widget_set_margin_start (list_view->details->columns_popover_box, 6);
    gtk_widget_set_margin_end (list_view->details->columns_popover_box, 6);
    gtk_popover_set_child (GTK_POPOVER (list_view->details->columns_popover),
                           list_view->details->columns_popover_box);

    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
                              G_CALLBACK (default_sort_order_changed_callback),
                              list_view);
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                              G_CALLBACK (default_sort_order_changed_callback),
                              list_view);
    g_signal_connect_swapped (nautilus_list_view_preferences,
                              "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
                              G_CALLBACK (default_visible_columns_changed_callback),
                              list_view);
    g_signal_connect_swapped (nautilus_list_view_preferences,
                              "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
                              G_CALLBACK (default_column_order_changed_callback),
                              list_view);

    /* React to clipboard changes */
    clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    g_signal_connect (clipboard, "owner-change",
                      G_CALLBACK (on_clipboard_owner_changed), list_view);

    nautilus_list_view_click_policy_changed (NAUTILUS_FILES_VIEW (list_view));

    nautilus_list_view_set_zoom_level (list_view, get_default_zoom_level ());

    list_view->details->hover_path = NULL;

    view_action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (list_view));
    g_action_map_add_action_entries (G_ACTION_MAP (view_action_group),
                                     list_view_entries,
                                     G_N_ELEMENTS (list_view_entries),
                                     list_view);
    /* Keep the action synced with the actual value, so the toolbar can poll it */
    g_action_group_change_action_state (nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (list_view)),
                                        "zoom-to-level", g_variant_new_int32 (get_default_zoom_level ()));

    list_view->details->regex = g_regex_new ("\\R+", 0, G_REGEX_MATCH_NEWLINE_ANY, NULL);

    list_view->details->tag_manager = nautilus_tag_manager_get ();
    list_view->details->starred_cancellable = g_cancellable_new ();

    g_signal_connect (list_view->details->tag_manager,
                      "starred-changed",
                      (GCallback) on_starred_files_changed,
                      list_view);
}

NautilusFilesView *
nautilus_list_view_new (NautilusWindowSlot *slot)
{
    return g_object_new (NAUTILUS_TYPE_LIST_VIEW,
                         "window-slot", slot,
                         NULL);
}
