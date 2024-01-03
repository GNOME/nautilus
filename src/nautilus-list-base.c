/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-list-base-private.h"

#include "nautilus-dnd.h"
#include "nautilus-view-cell.h"
#include "nautilus-view-item.h"
#include "nautilus-view-model.h"
#include "nautilus-files-view.h"
#include "nautilus-files-view-dnd.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-metadata.h"
#include "nautilus-global-preferences.h"
#include "nautilus-thumbnails.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/**
 * NautilusListBase:
 *
 * Abstract class containing shared code for #NautilusFilesView implementations
 * using a #GtkListBase-derived widget (e.g. GtkGridView, GtkColumnView) which
 * takes a #NautilusViewModel instance as its model and and a #NautilusViewCell
 * instance as #GtkListItem:child.
 *
 * It has been has been created to avoid code duplication in implementations,
 * while keeping #NautilusFilesView implementation-agnostic (should the need for
 * non-#GtkListBase views arise).
 */

typedef struct _NautilusListBasePrivate NautilusListBasePrivate;
struct _NautilusListBasePrivate
{
    NautilusViewModel *model;
    NautilusFile *directory_as_file;

    guint prioritize_thumbnailing_handle_id;
    GtkAdjustment *vadjustment;

    gboolean single_click_mode;
    gboolean activate_on_release;
    gboolean deny_background_click;

    GdkDragAction drag_item_action;
    GdkDragAction drag_view_action;
    graphene_point_t hover_start_point;
    guint hover_timer_id;
    GtkDropTarget *view_drop_target;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusListBase, nautilus_list_base, NAUTILUS_TYPE_FILES_VIEW)

enum
{
    PROP_0,
    PROP_ICON_SIZE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static inline NautilusViewItem *
get_view_item (GListModel *model,
               guint       position)
{
    g_autoptr (GtkTreeListRow) row = g_list_model_get_item (model, position);

    g_return_val_if_fail (GTK_IS_TREE_LIST_ROW (row), NULL);
    return NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
}

static inline void
internal_scroll_to (NautilusListBase   *self,
                    guint               position,
                    GtkListScrollFlags  flags,
                    GtkScrollInfo      *scroll)
{
    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->scroll_to (self, position, flags, scroll);
}

static guint
nautilus_list_base_get_icon_size (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_icon_size (self);
}

void
nautilus_list_base_set_cursor (NautilusListBase *self,
                               guint             position,
                               gboolean          select,
                               gboolean          scroll_to)
{
    GtkScrollInfo *info = gtk_scroll_info_new ();
    GtkListScrollFlags flags = (select ?
                                GTK_LIST_SCROLL_FOCUS | GTK_LIST_SCROLL_SELECT :
                                GTK_LIST_SCROLL_FOCUS);

    gtk_scroll_info_set_enable_vertical (info, scroll_to);
    gtk_scroll_info_set_enable_horizontal (info, scroll_to);

    internal_scroll_to (self, position, flags, info);
}

/* GtkListBase changes selection only with the primary button, and only after
 * release. But we need to anticipate selection earlier if we are to activate it
 * or open its context menu. This helper should be used in these situations if
 * it's desirable to act on a multi-item selection, because it preserves it. */
static void
select_single_item_if_not_selected (NautilusListBase *self,
                                    NautilusViewItem *item)
{
    NautilusViewModel *model;
    guint position;

    model = nautilus_list_base_get_model (self);
    position = nautilus_view_model_find (model, item);
    if (!gtk_selection_model_is_selected (GTK_SELECTION_MODEL (model), position))
    {
        nautilus_list_base_set_cursor (self, position, TRUE, FALSE);
    }
}

static void
activate_selection_on_click (NautilusListBase *self,
                             gboolean          open_in_new_tab)
{
    g_autolist (NautilusFile) selection = NULL;
    NautilusOpenFlags flags = 0;
    NautilusFilesView *files_view = NAUTILUS_FILES_VIEW (self);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    if (open_in_new_tab)
    {
        flags |= NAUTILUS_OPEN_FLAG_NEW_TAB;
        flags |= NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;
    }
    nautilus_files_view_activate_files (files_view, selection, flags, TRUE);
}

static void
open_context_menu_on_press (NautilusListBase *self,
                            NautilusViewCell *cell,
                            gdouble           x,
                            gdouble           y)
{
    g_autoptr (NautilusViewItem) item = NULL;
    gdouble view_x, view_y;

    item = nautilus_view_cell_get_item (cell);
    g_return_if_fail (item != NULL);

    /* Antecipate selection, if necessary. */
    select_single_item_if_not_selected (self, item);

    gtk_widget_translate_coordinates (GTK_WIDGET (cell), GTK_WIDGET (self),
                                      x, y,
                                      &view_x, &view_y);
    nautilus_files_view_pop_up_selection_context_menu (NAUTILUS_FILES_VIEW (self),
                                                       view_x, view_y);
}

static void
rubberband_set_state (NautilusListBase *self,
                      gboolean          enabled)
{
    /* This is a temporary workaround to deal with the rubberbanding issues
     * during a drag and drop. Disable rubberband on item press and enable
     * rubberband on item release/stop. See:
     * https://gitlab.gnome.org/GNOME/gtk/-/issues/5670 */

    GtkWidget *view;

    view = NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_view_ui (self);
    if (GTK_IS_GRID_VIEW (view))
    {
        gtk_grid_view_set_enable_rubberband (GTK_GRID_VIEW (view), enabled);
    }
    else if (GTK_IS_COLUMN_VIEW (view))
    {
        gtk_column_view_set_enable_rubberband (GTK_COLUMN_VIEW (view), enabled);
    }
}

static void
on_item_click_pressed (GtkGestureClick *gesture,
                       gint             n_press,
                       gdouble          x,
                       gdouble          y,
                       gpointer         user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint button;
    GdkModifierType modifiers;
    gboolean selection_mode;

    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    modifiers = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
    selection_mode = (modifiers & (GDK_CONTROL_MASK | GDK_SHIFT_MASK));

    /* Before anything else, store event state to be read by other handlers. */
    priv->deny_background_click = TRUE;
    priv->activate_on_release = (priv->single_click_mode &&
                                 button == GDK_BUTTON_PRIMARY &&
                                 n_press == 1 &&
                                 !selection_mode);

    rubberband_set_state (self, FALSE);

    /* It's safe to claim event sequence on press in the following cases because
     * they don't interfere with touch scrolling. */
    if (button == GDK_BUTTON_PRIMARY && n_press == 2 && !priv->single_click_mode)
    {
        /* If Ctrl + Shift are held, we don't want to activate selection. But
         * we still need to claim the event, otherwise GtkListBase's default
         * gesture is going to trigger activation. */
        if (!selection_mode)
        {
            activate_selection_on_click (self, FALSE);
        }
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
    else if (button == GDK_BUTTON_MIDDLE && n_press == 1)
    {
        g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);
        g_return_if_fail (item != NULL);

        /* Anticipate selection, if necessary, to activate it. */
        select_single_item_if_not_selected (self, item);
        activate_selection_on_click (self, TRUE);
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
    else if (button == GDK_BUTTON_SECONDARY && n_press == 1)
    {
        open_context_menu_on_press (self, cell, x, y);
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void
on_item_click_released (GtkGestureClick *gesture,
                        gint             n_press,
                        gdouble          x,
                        gdouble          y,
                        gpointer         user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    if (priv->activate_on_release)
    {
        NautilusViewModel *model;
        g_autoptr (NautilusViewItem) item = NULL;
        guint i;

        model = nautilus_list_base_get_model (self);
        item = nautilus_view_cell_get_item (cell);
        g_return_if_fail (item != NULL);
        i = nautilus_view_model_find (model, item);

        /* Anticipate selection, enforcing single selection of target item. */
        gtk_selection_model_select_item (GTK_SELECTION_MODEL (model), i, TRUE);

        activate_selection_on_click (self, FALSE);
    }

    rubberband_set_state (self, TRUE);
    priv->activate_on_release = FALSE;
    priv->deny_background_click = FALSE;
}

static void
on_item_click_stopped (GtkGestureClick *gesture,
                       gpointer         user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    if (self == NULL)
    {
        /* The view may already be gone before the cell finalized. */
        return;
    }

    rubberband_set_state (self, TRUE);
    priv->activate_on_release = FALSE;
    priv->deny_background_click = FALSE;
}

static void
on_view_click_pressed (GtkGestureClick *gesture,
                       gint             n_press,
                       gdouble          x,
                       gdouble          y,
                       gpointer         user_data)
{
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint button;
    GdkModifierType modifiers;
    gboolean selection_mode;

    if (priv->deny_background_click)
    {
        /* Item was clicked. */
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
        return;
    }

    /* We are overriding many of the gestures for the views so let's make sure to
     * grab the focus in order to make rubberbanding and background click work */
    gtk_widget_grab_focus (GTK_WIDGET (self));

    /* Don't interfere with GtkListBase default selection handling when
     * holding Ctrl and Shift. */
    modifiers = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
    selection_mode = (modifiers & (GDK_CONTROL_MASK | GDK_SHIFT_MASK));
    if (!selection_mode)
    {
        nautilus_view_set_selection (NAUTILUS_VIEW (self), NULL);
    }

    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    if (button == GDK_BUTTON_SECONDARY)
    {
        GtkWidget *event_widget;
        gdouble view_x;
        gdouble view_y;

        event_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
        gtk_widget_translate_coordinates (event_widget, GTK_WIDGET (self),
                                          x, y,
                                          &view_x, &view_y);
        nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (self),
                                                            view_x, view_y);
    }
}

static void
on_item_longpress_pressed (GtkGestureLongPress *gesture,
                           gdouble              x,
                           gdouble              y,
                           gpointer             user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);

    open_context_menu_on_press (self, cell, x, y);
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_view_longpress_pressed (GtkGestureLongPress *gesture,
                           gdouble              x,
                           gdouble              y,
                           gpointer             user_data)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (user_data);
    GtkWidget *event_widget;
    gdouble view_x;
    gdouble view_y;

    event_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

    gtk_widget_translate_coordinates (event_widget,
                                      GTK_WIDGET (self),
                                      x, y, &view_x, &view_y);

    nautilus_view_set_selection (NAUTILUS_VIEW (self), NULL);
    nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (self),
                                                        view_x, view_y);
}

static GdkContentProvider *
on_item_drag_prepare (GtkDragSource *source,
                      double         x,
                      double         y,
                      gpointer       user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    GtkWidget *view_ui;
    g_autolist (NautilusFile) selection = NULL;
    g_autoslist (GFile) file_list = NULL;
    g_autoptr (GdkPaintable) paintable = NULL;
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);
    GdkDragAction actions;
    gint scale_factor;

    /* Anticipate selection, if necessary, for dragging the clicked item. */
    select_single_item_if_not_selected (self, item);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    g_return_val_if_fail (selection != NULL, NULL);

    gtk_gesture_set_state (GTK_GESTURE (source), GTK_EVENT_SEQUENCE_CLAIMED);

    actions = GDK_ACTION_ALL | GDK_ACTION_ASK;

    for (GList *l = selection; l != NULL; l = l->next)
    {
        /* Convert to GTK_TYPE_FILE_LIST, which is assumed to be a GSList<GFile>. */
        file_list = g_slist_prepend (file_list, nautilus_file_get_activation_location (l->data));

        if (!nautilus_file_can_delete (l->data))
        {
            actions &= ~GDK_ACTION_MOVE;
        }
    }
    file_list = g_slist_reverse (file_list);

    gtk_drag_source_set_actions (source, actions);

    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    paintable = get_paintable_for_drag_selection (selection, scale_factor);

    view_ui = NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_view_ui (self);
    if (GTK_IS_GRID_VIEW (view_ui))
    {
        x = x * NAUTILUS_DRAG_SURFACE_ICON_SIZE / nautilus_list_base_get_icon_size (self);
        y = y * NAUTILUS_DRAG_SURFACE_ICON_SIZE / nautilus_list_base_get_icon_size (self);
    }
    else
    {
        x = 0;
        y = 0;
    }

    gtk_drag_source_set_icon (source, paintable, x, y);

    return gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST, file_list);
}

static gboolean
hover_timer (gpointer user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);
    g_autofree gchar *uri = NULL;

    priv->hover_timer_id = 0;

    if (priv->drag_item_action == 0)
    {
        /* If we aren't able to dropped don't change the location. This stops
         * drops onto themselves, and another unnecessary drops. */
        return G_SOURCE_REMOVE;
    }

    uri = nautilus_file_get_uri (nautilus_view_item_get_file (item));
    nautilus_files_view_handle_hover (NAUTILUS_FILES_VIEW (self), uri);

    return G_SOURCE_REMOVE;
}

static void
on_item_drag_hover_enter (GtkDropControllerMotion *controller,
                          gdouble                  x,
                          gdouble                  y,
                          gpointer                 user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    priv->hover_start_point.x = x;
    priv->hover_start_point.y = y;
}

static void
on_item_drag_hover_leave (GtkDropControllerMotion *controller,
                          gpointer                 user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    g_clear_handle_id (&priv->hover_timer_id, g_source_remove);
}

static void
on_item_drag_hover_motion (GtkDropControllerMotion *controller,
                           gdouble                  x,
                           gdouble                  y,
                           gpointer                 user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    graphene_point_t start = priv->hover_start_point;

    /* This condition doubles in two roles:
     *   - If the timeout hasn't started yet, to ensure the pointer has entered
     *     deep enough into the cell before starting the timeout to switch;
     *   - If the timeout has already started, to reset it if the pointer is
     *     moving a lot.
     * Both serve to prevent accidental triggering of switch-on-hover. */
    if (gtk_drag_check_threshold (GTK_WIDGET (cell), start.x, start.y, x, y))
    {
        g_clear_handle_id (&priv->hover_timer_id, g_source_remove);
        priv->hover_timer_id = g_timeout_add (HOVER_TIMEOUT, hover_timer, cell);
        priv->hover_start_point.x = x;
        priv->hover_start_point.y = y;
    }
}

static GdkDragAction
get_preferred_action (NautilusFile *target_file,
                      const GValue *value)
{
    GdkDragAction action = 0;

    if (value == NULL)
    {
        action = nautilus_dnd_get_preferred_action (target_file, NULL);
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *source_file_list = g_value_get_boxed (value);
        if (source_file_list != NULL)
        {
            action = nautilus_dnd_get_preferred_action (target_file, source_file_list->data);
        }
        else
        {
            action = nautilus_dnd_get_preferred_action (target_file, NULL);
        }
    }
    else if (G_VALUE_HOLDS (value, G_TYPE_STRING) || G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
    {
        action = GDK_ACTION_COPY;
    }

    return action;
}

static GdkDragAction
on_item_drag_enter (GtkDropTarget *target,
                    double         x,
                    double         y,
                    gpointer       user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = NULL;
    const GValue *value;
    g_autoptr (NautilusFile) dest_file = NULL;

    /* Reset action cache. */
    priv->drag_item_action = 0;

    item = nautilus_view_cell_get_item (cell);
    if (item == NULL)
    {
        gtk_drop_target_reject (target);
        return 0;
    }

    dest_file = nautilus_file_ref (nautilus_view_item_get_file (item));

    if (!nautilus_file_is_archive (dest_file) && !nautilus_file_is_directory (dest_file))
    {
        gtk_drop_target_reject (target);
        return 0;
    }

    value = gtk_drop_target_get_value (target);
    priv->drag_item_action = get_preferred_action (dest_file, value);
    if (priv->drag_item_action == 0)
    {
        gtk_drop_target_reject (target);
        return 0;
    }

    nautilus_view_item_set_drag_accept (item, TRUE);
    return priv->drag_item_action;
}

static void
on_item_drag_value_notify (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    GtkDropTarget *target = GTK_DROP_TARGET (object);
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    const GValue *value;
    g_autoptr (NautilusViewItem) item = NULL;

    value = gtk_drop_target_get_value (target);
    if (value == NULL)
    {
        return;
    }

    item = nautilus_view_cell_get_item (cell);
    g_return_if_fail (NAUTILUS_IS_VIEW_ITEM (item));

    priv->drag_item_action = get_preferred_action (nautilus_view_item_get_file (item), value);
}

static GdkDragAction
on_item_drag_motion (GtkDropTarget *target,
                     double         x,
                     double         y,
                     gpointer       user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    /* There's a bug in GtkDropTarget where motion overrides enter
     * so until we fix that let's just return the action that we already
     * received from enter*/

    return priv->drag_item_action;
}

static void
on_item_drag_leave (GtkDropTarget *dest,
                    gpointer       user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);

    nautilus_view_item_set_drag_accept (item, FALSE);
}

static gboolean
on_item_drop (GtkDropTarget *target,
              const GValue  *value,
              double         x,
              double         y,
              gpointer       user_data)
{
    NautilusViewCell *cell = user_data;
    g_autoptr (NautilusListBase) self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);
    GdkDragAction actions;
    g_autoptr (GFile) target_location = nautilus_file_get_location (nautilus_view_item_get_file (item));

    actions = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));

    #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (self))))
    {
        /* Temporary workaround until the below GTK MR (or equivalend fix)
         * is merged.  Without this fix, the preferred action isn't set correctly.
         * https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4982 */
        GdkDrag *drag = gdk_drop_get_drag (gtk_drop_target_get_current_drop (target));
        actions = drag != NULL ? gdk_drag_get_selected_action (drag) : GDK_ACTION_COPY;
    }
    #endif

    /* In x11 the leave signal isn't emitted on a drop so we need to clear the timeout */
    g_clear_handle_id (&priv->hover_timer_id, g_source_remove);

    return nautilus_dnd_perform_drop (NAUTILUS_FILES_VIEW (self), value, actions, target_location);
}

static GdkDragAction
on_view_drag_enter (GtkDropTarget *target,
                    double         x,
                    double         y,
                    gpointer       user_data)
{
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    NautilusFile *dest_file = nautilus_list_base_get_directory_as_file (self);
    const GValue *value;

    value = gtk_drop_target_get_value (target);
    priv->drag_view_action = get_preferred_action (dest_file, value);
    if (priv->drag_view_action == 0)
    {
        /* Don't summarily reject because the view's location might change on
         * hover, so a DND action may become available. */
        return 0;
    }

    return priv->drag_view_action;
}

static void
on_view_drag_value_notify (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    GtkDropTarget *target = GTK_DROP_TARGET (object);
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    const GValue *value;
    NautilusFile *dest_file = nautilus_list_base_get_directory_as_file (self);

    value = gtk_drop_target_get_value (target);
    if (value == NULL)
    {
        return;
    }

    priv->drag_view_action = get_preferred_action (dest_file, value);
}

static GdkDragAction
on_view_drag_motion (GtkDropTarget *target,
                     double         x,
                     double         y,
                     gpointer       user_data)
{
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    return priv->drag_view_action;
}

static gboolean
on_view_drop (GtkDropTarget *target,
              const GValue  *value,
              double         x,
              double         y,
              gpointer       user_data)
{
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GdkDragAction actions;
    g_autoptr (GFile) target_location = NULL;

    if (priv->drag_view_action == 0)
    {
        /* We didn't reject earlier because the view's location may change and,
         * as a result, a drop action might become available. */
        return FALSE;
    }

    actions = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));
    target_location = nautilus_view_get_location (NAUTILUS_VIEW (self));

    #ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (self))))
    {
        /* Temporary workaround until the below GTK MR (or equivalend fix)
         * is merged.  Without this fix, the preferred action isn't set correctly.
         * https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4982 */
        GdkDrag *drag = gdk_drop_get_drag (gtk_drop_target_get_current_drop (target));
        actions = drag != NULL ? gdk_drag_get_selected_action (drag) : GDK_ACTION_COPY;
    }
    #endif

    return nautilus_dnd_perform_drop (NAUTILUS_FILES_VIEW (self), value, actions, target_location);
}

void
setup_cell_common (GObject          *listitem,
                   NautilusViewCell *cell)
{
    GtkExpression *expression;
    GtkEventController *controller;
    GtkDropTarget *drop_target;

    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    gtk_expression_bind (expression, cell, "item", listitem);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (cell), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed", G_CALLBACK (on_item_click_pressed), cell);
    g_signal_connect (controller, "stopped", G_CALLBACK (on_item_click_stopped), cell);
    g_signal_connect (controller, "released", G_CALLBACK (on_item_click_released), cell);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (GTK_WIDGET (cell), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed", G_CALLBACK (on_item_longpress_pressed), cell);

    controller = GTK_EVENT_CONTROLLER (gtk_drag_source_new ());
    gtk_widget_add_controller (GTK_WIDGET (cell), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    g_signal_connect (controller, "prepare", G_CALLBACK (on_item_drag_prepare), cell);

    /* TODO: Implement GDK_ACTION_ASK */
    drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);
    gtk_drop_target_set_preload (drop_target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (drop_target, (GType[3]) { GDK_TYPE_TEXTURE, GDK_TYPE_FILE_LIST, G_TYPE_STRING }, 3);
    g_signal_connect (drop_target, "enter", G_CALLBACK (on_item_drag_enter), cell);
    g_signal_connect (drop_target, "notify::value", G_CALLBACK (on_item_drag_value_notify), cell);
    g_signal_connect (drop_target, "leave", G_CALLBACK (on_item_drag_leave), cell);
    g_signal_connect (drop_target, "motion", G_CALLBACK (on_item_drag_motion), cell);
    g_signal_connect (drop_target, "drop", G_CALLBACK (on_item_drop), cell);
    gtk_widget_add_controller (GTK_WIDGET (cell), GTK_EVENT_CONTROLLER (drop_target));
}

static void
real_setup_cell_hover (NautilusViewCell *cell,
                       GtkWidget        *target)
{
    GtkEventController *controller = gtk_drop_controller_motion_new ();
    gtk_widget_add_controller (target, controller);
    g_signal_connect (controller, "enter", G_CALLBACK (on_item_drag_hover_enter), cell);
    g_signal_connect (controller, "leave", G_CALLBACK (on_item_drag_hover_leave), cell);
    g_signal_connect (controller, "motion", G_CALLBACK (on_item_drag_hover_motion), cell);
}

void
setup_cell_hover_inner_target (NautilusViewCell *cell,
                               GtkWidget        *target)
{
    g_return_if_fail (gtk_widget_is_ancestor (target, GTK_WIDGET (cell)));

    real_setup_cell_hover (cell, target);
}

void
setup_cell_hover (NautilusViewCell *cell)
{
    real_setup_cell_hover (cell, GTK_WIDGET (cell));
}

static void
nautilus_list_base_scroll_to_item (NautilusListBase *self,
                                   guint             position)
{
    internal_scroll_to (self, position, GTK_LIST_SCROLL_NONE, NULL);
}

static GtkWidget *
nautilus_list_base_get_view_ui (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_view_ui (self);
}

typedef struct
{
    NautilusListBase *self;
    GQuark attribute_q;
} NautilusListBaseSortData;

static void
base_setup_directory (NautilusListBase  *self,
                      NautilusDirectory *directory)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    g_clear_object (&priv->directory_as_file);
    priv->directory_as_file = nautilus_directory_get_corresponding_file (directory);

    /* Temporary workaround */
    rubberband_set_state (self, TRUE);

    /* When double clicking on an item this deny_background_click can persist
     * because the new view interrupts the gesture sequence, so lets reset it.*/
    priv->deny_background_click = FALSE;

    /* When DnD is used to navigate between directories, the normal callbacks
     * are ignored. Update DnD variables here upon navigating to a directory*/
    if (gtk_drop_target_get_current_drop (priv->view_drop_target) != NULL)
    {
        priv->drag_view_action = get_preferred_action (nautilus_list_base_get_directory_as_file (self),
                                                       gtk_drop_target_get_value (priv->view_drop_target));
        priv->drag_item_action = 0;
    }
}

static void
set_click_mode_from_settings (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    int click_policy;

    click_policy = g_settings_get_enum (nautilus_preferences,
                                        NAUTILUS_PREFERENCES_CLICK_POLICY);

    priv->single_click_mode = (click_policy == NAUTILUS_CLICK_POLICY_SINGLE);
}

static guint
get_first_selected_item (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autolist (NautilusFile) selection = NULL;
    NautilusFile *file;
    NautilusViewItem *item;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    if (selection == NULL)
    {
        return G_MAXUINT;
    }

    file = NAUTILUS_FILE (selection->data);
    item = nautilus_view_model_get_item_for_file (priv->model, file);

    return nautilus_view_model_find (priv->model, item);
}

static void
real_reveal_selection (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);

    nautilus_list_base_scroll_to_item (self, get_first_selected_item (self));
}

static guint
get_first_visible_item (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint n_items;
    GtkWidget *view_ui;
    GtkBorder border = {0};

    n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->model));
    view_ui = nautilus_list_base_get_view_ui (self);
    gtk_scrollable_get_border (GTK_SCROLLABLE (view_ui), &border);

    for (guint i = 0; i < n_items; i++)
    {
        g_autoptr (NautilusViewItem) item = NULL;
        GtkWidget *item_ui;

        item = get_view_item (G_LIST_MODEL (priv->model), i);
        item_ui = nautilus_view_item_get_item_ui (item);
        if (item_ui != NULL && gtk_widget_get_mapped (item_ui))
        {
            GtkWidget *list_item_widget = gtk_widget_get_parent (item_ui);
            gdouble h = gtk_widget_get_height (list_item_widget);
            gdouble y;

            gtk_widget_translate_coordinates (list_item_widget, GTK_WIDGET (self),
                                              0, h, NULL, &y);
            if (y >= border.top)
            {
                return i;
            }
        }
    }

    return G_MAXUINT;
}

static guint
get_last_visible_item (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint n_items;
    GtkWidget *view_ui;
    GtkBorder border = {0};
    gdouble scroll_h;

    n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->model));
    view_ui = nautilus_list_base_get_view_ui (self);
    gtk_scrollable_get_border (GTK_SCROLLABLE (view_ui), &border);
    scroll_h = gtk_widget_get_allocated_height (GTK_WIDGET (self)) - border.bottom;

    for (guint i = (n_items - 1); (i + 1) > 0; i--)
    {
        g_autoptr (NautilusViewItem) item = NULL;
        GtkWidget *item_ui;

        item = get_view_item (G_LIST_MODEL (priv->model), i);
        item_ui = nautilus_view_item_get_item_ui (item);
        if (item_ui != NULL && gtk_widget_get_mapped (item_ui))
        {
            GtkWidget *list_item_widget = gtk_widget_get_parent (item_ui);
            gdouble h = gtk_widget_get_allocated_height (list_item_widget);
            gdouble y;

            gtk_widget_translate_coordinates (list_item_widget, GTK_WIDGET (self),
                                              0, h, NULL, &y);
            if (y <= scroll_h)
            {
                return i;
            }
        }
    }

    return G_MAXUINT;
}

static char *
real_get_first_visible_file (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint i;
    g_autoptr (NautilusViewItem) item = NULL;
    gchar *uri = NULL;

    i = get_first_visible_item (self);
    if (i < G_MAXUINT)
    {
        item = get_view_item (G_LIST_MODEL (priv->model), i);
        uri = nautilus_file_get_uri (nautilus_view_item_get_file (item));
    }
    return uri;
}

static char *
real_get_last_visible_file (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint i;
    g_autoptr (NautilusViewItem) item = NULL;
    gchar *uri = NULL;

    i = get_last_visible_item (self);
    if (i < G_MAXUINT)
    {
        item = get_view_item (G_LIST_MODEL (priv->model), i);
        uri = nautilus_file_get_uri (nautilus_view_item_get_file (item));
    }
    return uri;
}

static void
real_scroll_to_file (NautilusFilesView *files_view,
                     const char        *uri)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusFile) file = nautilus_file_get_existing_by_uri (uri);
    NautilusViewItem *item;
    guint i;

    item = nautilus_view_model_get_item_for_file (priv->model, file);
    g_return_if_fail (item != NULL);

    i = nautilus_view_model_find (priv->model, item);
    nautilus_list_base_scroll_to_item (self, i);
}

static GdkRectangle *
get_rectangle_for_item_ui (NautilusListBase *self,
                           GtkWidget        *item_ui)
{
    GdkRectangle *rectangle;
    GtkWidget *content_widget;
    gdouble view_x;
    gdouble view_y;

    rectangle = g_new0 (GdkRectangle, 1);
    gtk_widget_get_allocation (item_ui, rectangle);

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    gtk_widget_translate_coordinates (item_ui, content_widget,
                                      rectangle->x, rectangle->y,
                                      &view_x, &view_y);
    rectangle->x = view_x;
    rectangle->y = view_y;

    return rectangle;
}

static GdkRectangle *
real_compute_rename_popover_pointing_to (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = NULL;
    GtkWidget *item_ui;

    /* We only allow one item to be renamed with a popover */
    item = get_view_item (G_LIST_MODEL (priv->model), get_first_selected_item (self));
    item_ui = nautilus_view_item_get_item_ui (item);
    g_return_val_if_fail (item_ui != NULL, NULL);

    return get_rectangle_for_item_ui (self, item_ui);
}

/**
 * Gets area to popup the context menu from, ensuring it is currently visible.
 *
 * Don't use this if context menu is triggered by a pointing device (in which
 * case it should popup from the position of the pointer). Instead, this is
 * meant for non-pointer case, such as using the Menu key from the keyboard.
 **/
static GdkRectangle *
real_reveal_for_selection_context_menu (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GtkBitset) selection = NULL;
    guint i;
    g_autoptr (NautilusViewItem) item = NULL;
    GtkWidget *item_ui;

    selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    g_return_val_if_fail (!gtk_bitset_is_empty (selection), NULL);

    /* If multiple items are selected, we need to pick one. The ideal choice is
     * the currently focused item, if it is part of the selection.
     *
     * (Sadly, that's not currently possible, because GTK doesn't let us know
     * the focus position: https://gitlab.gnome.org/GNOME/gtk/-/issues/2891 )
     *
     * Otherwise, get the selected item_ui which is sorted the lowest.*/
    i = gtk_bitset_get_minimum (selection);

    nautilus_list_base_scroll_to_item (self, i);

    item = get_view_item (G_LIST_MODEL (priv->model), i);
    item_ui = nautilus_view_item_get_item_ui (item);
    g_return_val_if_fail (item_ui != NULL, NULL);

    return get_rectangle_for_item_ui (self, item_ui);
}

static void
default_preview_selection_event (NautilusListBase *self,
                                 GtkDirectionType  direction)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint i;
    gboolean rtl = (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL);

    i = get_first_selected_item (self);
    if (direction == GTK_DIR_UP ||
        direction == (rtl ? GTK_DIR_RIGHT : GTK_DIR_LEFT))
    {
        if (i == 0)
        {
            return;
        }

        i--;
    }
    else
    {
        i++;

        if (i >= g_list_model_get_n_items (G_LIST_MODEL (priv->model)))
        {
            return;
        }
    }

    nautilus_list_base_set_cursor (self, i, TRUE, TRUE);
}

static void
nautilus_list_base_dispose (GObject *object)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (object);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    g_clear_object (&priv->directory_as_file);
    g_clear_handle_id (&priv->prioritize_thumbnailing_handle_id, g_source_remove);
    g_clear_handle_id (&priv->hover_timer_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_list_base_parent_class)->dispose (object);
}

static void
nautilus_list_base_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_list_base_parent_class)->finalize (object);
}

static gboolean
prioritize_thumbnailing_on_idle (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    gdouble page_size;
    GtkWidget *first_visible_child;
    GtkWidget *next_child;
    guint first_index;
    guint next_index;
    gdouble y;
    guint last_index;
    g_autoptr (NautilusViewItem) first_item = NULL;
    NautilusFile *file;

    priv->prioritize_thumbnailing_handle_id = 0;

    page_size = gtk_adjustment_get_page_size (priv->vadjustment);
    first_index = get_first_visible_item (self);
    if (first_index == G_MAXUINT)
    {
        return G_SOURCE_REMOVE;
    }

    first_item = get_view_item (G_LIST_MODEL (priv->model), first_index);

    first_visible_child = nautilus_view_item_get_item_ui (first_item);

    for (next_index = first_index + 1; next_index < g_list_model_get_n_items (G_LIST_MODEL (priv->model)); next_index++)
    {
        g_autoptr (NautilusViewItem) next_item = NULL;

        next_item = get_view_item (G_LIST_MODEL (priv->model), next_index);
        next_child = nautilus_view_item_get_item_ui (next_item);
        if (next_child == NULL)
        {
            break;
        }
        if (gtk_widget_translate_coordinates (next_child, first_visible_child,
                                              0, 0, NULL, &y))
        {
            if (y > page_size)
            {
                break;
            }
        }
    }
    last_index = next_index - 1;

    /* Do the iteration in reverse to give higher priority to the top */
    for (guint i = 0; i <= last_index - first_index; i++)
    {
        g_autoptr (NautilusViewItem) item = NULL;

        item = get_view_item (G_LIST_MODEL (priv->model), last_index - i);
        g_return_val_if_fail (item != NULL, G_SOURCE_REMOVE);

        file = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM (item));
        if (file != NULL && nautilus_file_is_thumbnailing (file))
        {
            g_autofree gchar *uri = nautilus_file_get_uri (file);
            nautilus_thumbnail_prioritize (uri);
        }
    }

    return G_SOURCE_REMOVE;
}

static void
on_vadjustment_changed (GtkAdjustment *adjustment,
                        gpointer       user_data)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (user_data);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint handle_id;

    /* Schedule on idle to rate limit and to avoid delaying scrolling. */
    if (priv->prioritize_thumbnailing_handle_id == 0)
    {
        handle_id = g_idle_add ((GSourceFunc) prioritize_thumbnailing_on_idle, self);
        priv->prioritize_thumbnailing_handle_id = handle_id;
    }
}

static gboolean
nautilus_list_base_focus (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (widget);
    g_autolist (NautilusFile) selection = NULL;
    gboolean no_selection;
    gboolean handled;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    no_selection = (selection == NULL);

    handled = GTK_WIDGET_CLASS (nautilus_list_base_parent_class)->focus (widget, direction);

    if (handled && no_selection)
    {
        GtkWidget *focus_widget = gtk_root_get_focus (gtk_widget_get_root (widget));

        /* Workaround for https://gitlab.gnome.org/GNOME/nautilus/-/issues/2489
         * Also ensures an item gets selected when using <Tab> to focus the view.
         * Ideally to be fixed in GtkListBase instead. */
        if (focus_widget != NULL)
        {
            gtk_widget_activate_action (focus_widget,
                                        "listitem.select",
                                        "(bb)",
                                        FALSE, FALSE);
        }
    }

    return handled;
}

static void
nautilus_list_base_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (object);

    switch (prop_id)
    {
        case PROP_ICON_SIZE:
        {
            g_value_set_uint (value, nautilus_list_base_get_icon_size (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_list_base_class_init (NautilusListBaseClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    NautilusFilesViewClass *files_view_class = NAUTILUS_FILES_VIEW_CLASS (klass);

    object_class->dispose = nautilus_list_base_dispose;
    object_class->finalize = nautilus_list_base_finalize;
    object_class->get_property = nautilus_list_base_get_property;

    widget_class->focus = nautilus_list_base_focus;

    files_view_class->get_first_visible_file = real_get_first_visible_file;
    files_view_class->get_last_visible_file = real_get_last_visible_file;
    files_view_class->reveal_selection = real_reveal_selection;
    files_view_class->scroll_to_file = real_scroll_to_file;
    files_view_class->compute_rename_popover_pointing_to = real_compute_rename_popover_pointing_to;
    files_view_class->reveal_for_selection_context_menu = real_reveal_for_selection_context_menu;

    klass->preview_selection_event = default_preview_selection_event;
    klass->setup_directory = base_setup_directory;

    properties[PROP_ICON_SIZE] = g_param_spec_uint ("icon-size",
                                                    "", "",
                                                    NAUTILUS_LIST_ICON_SIZE_SMALL,
                                                    NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE,
                                                    NAUTILUS_GRID_ICON_SIZE_LARGE,
                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
nautilus_list_base_init (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkWidget *content_widget;
    GtkAdjustment *vadjustment;

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (content_widget));

    priv->vadjustment = vadjustment;
    g_signal_connect (vadjustment, "changed", (GCallback) on_vadjustment_changed, self);
    g_signal_connect (vadjustment, "value-changed", (GCallback) on_vadjustment_changed, self);

    priv->model = NAUTILUS_VIEW_MODEL (nautilus_files_view_get_model (NAUTILUS_FILES_VIEW (self)));

    g_signal_connect_object (nautilus_preferences,
                             "changed::" NAUTILUS_PREFERENCES_CLICK_POLICY,
                             G_CALLBACK (set_click_mode_from_settings), self,
                             G_CONNECT_SWAPPED);
    set_click_mode_from_settings (self);
}

NautilusFile *
nautilus_list_base_get_directory_as_file (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    return priv->directory_as_file;
}

NautilusViewModel *
nautilus_list_base_get_model (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    return priv->model;
}

void
nautilus_list_base_setup_gestures (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkEventController *controller;
    GtkDropTarget *drop_target;

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_view_click_pressed), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (GTK_WIDGET (self), controller);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_view_longpress_pressed), self);

    /* TODO: Implement GDK_ACTION_ASK */
    drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);
    gtk_drop_target_set_preload (drop_target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (drop_target, (GType[3]) { GDK_TYPE_TEXTURE, GDK_TYPE_FILE_LIST, G_TYPE_STRING }, 3);
    g_signal_connect (drop_target, "enter", G_CALLBACK (on_view_drag_enter), self);
    g_signal_connect (drop_target, "notify::value", G_CALLBACK (on_view_drag_value_notify), self);
    g_signal_connect (drop_target, "motion", G_CALLBACK (on_view_drag_motion), self);
    g_signal_connect (drop_target, "drop", G_CALLBACK (on_view_drop), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
    priv->view_drop_target = drop_target;
}

NautilusViewInfo
nautilus_list_base_get_view_info (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_view_info (self);
}

int
nautilus_list_base_get_zoom_level (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_zoom_level (self);
}

void
nautilus_list_base_preview_selection_event (NautilusListBase *self,
                                            GtkDirectionType  direction)
{
    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->preview_selection_event (self, direction);
}

/* This should be called when changing view directory, but only once information
 * on the new directory file is ready, becase we need it to define sorting,
 * drop action. etc. We defer on NautilusFilesView the responsibility of calling
 * this at the right time, which, at the time of writing, is the default hanlder
 * of the NautilusFilesView::begin-loading signal. */
void
nautilus_list_base_setup_directory (NautilusListBase  *self,
                                    NautilusDirectory *directory)
{
    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->setup_directory (self, directory);
}
