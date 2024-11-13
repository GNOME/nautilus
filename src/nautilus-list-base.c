/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-list-base-private.h"

#include "nautilus-application.h"
#include "nautilus-dnd.h"
#include "nautilus-view-cell.h"
#include "nautilus-view-item.h"
#include "nautilus-view-model.h"
#include "nautilus-enum-types.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-metadata.h"
#include "nautilus-global-preferences.h"
#include "nautilus-thumbnails.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/* 1 page worth of scroll in 100ms zooms in or out when the ctrl key is held */
#define SCROLL_TO_ZOOM_INTERVAL 100

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

    GtkWidget *overlay;
    GtkWidget *scrolled_window;

    gboolean dnd_disabled;
    gboolean single_click_mode;

    gboolean activate_on_release;
    gboolean deny_background_click;

    GdkDragAction drag_item_action;
    GdkDragAction drag_view_action;
    graphene_point_t hover_start_point;
    guint hover_timer_id;
    GtkDropTarget *view_drop_target;

    gdouble amount_scrolled_for_zoom;
    guint scroll_timeout_id;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusListBase, nautilus_list_base, ADW_TYPE_BIN)

enum
{
    ACTIVATE_SELECTION,
    PERFORM_DROP,
    POPUP_BACKGROUND_CONTEXT_MENU,
    POPUP_SELECTION_CONTEXT_MENU,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
    PROP_0,
    PROP_ICON_SIZE,
    PROP_MODEL,
    PROP_SORT_STATE,
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

GVariant *
nautilus_list_base_get_sort_state (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_sort_state (self);
}

void
nautilus_list_base_set_model (NautilusListBase  *self,
                              NautilusViewModel *model)
{
    g_object_set (self, "model", model, NULL);
}

void
nautilus_list_base_set_sort_state (NautilusListBase *self,
                                   GVariant         *sort_state)
{
    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->set_sort_state (self, sort_state);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORT_STATE]);
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
                                    NautilusViewCell *cell)
{
    NautilusViewModel *model;
    guint position = nautilus_view_cell_get_position (cell);

    model = nautilus_list_base_get_model (self);
    if (!gtk_selection_model_is_selected (GTK_SELECTION_MODEL (model), position))
    {
        nautilus_list_base_set_cursor (self, position, TRUE, FALSE);
    }
}

void
nautilus_list_base_activate_selection (NautilusListBase *self,
                                       gboolean          open_in_new_tab)
{
    NautilusOpenFlags flags = 0;

    if (open_in_new_tab)
    {
        flags |= NAUTILUS_OPEN_FLAG_NEW_TAB;
        flags |= NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;
    }

    g_signal_emit (self, signals[ACTIVATE_SELECTION], 0, flags);
}

static void
open_context_menu_on_press (NautilusListBase *self,
                            NautilusViewCell *cell,
                            gdouble           x,
                            gdouble           y)
{
    /* Antecipate selection, if necessary. */
    select_single_item_if_not_selected (self, cell);

    g_signal_emit (self, signals[POPUP_SELECTION_CONTEXT_MENU], 0, x, y, GTK_WIDGET (cell));
}

static void
rubberband_set_state (NautilusListBase *self,
                      gboolean          enabled)
{
    /* This is a temporary workaround to deal with the rubberbanding issues
     * during a drag and drop. Disable rubberband on item press and enable
     * rubberband on item release/stop. See:
     * https://gitlab.gnome.org/GNOME/gtk/-/issues/5670 */

    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    if (priv->model != NULL && nautilus_view_model_get_single_selection (priv->model))
    {
        /* Rubberband is always disabled in this case. Do nothing. */
        return;
    }

    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->set_enable_rubberband (self, enabled);
}

/**
 * nautilus_list_base_add_overlay:
 * @self: a `NautilusListBase` instance
 * @widget: a `GtkWidget` to be added
 *
 * Adds @widget as an overlay to the view. This allows the view event handling,
 * such as secondary clicks and drops, to work as expected even if the pointer
 * is on the overlay.
 *
 * The primary use case is the empty status page.
 */
void
nautilus_list_base_add_overlay (NautilusListBase *self,
                                GtkWidget        *widget)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay), widget);
}

static void
on_item_click_pressed (GtkGestureClick *gesture,
                       gint             n_press,
                       gdouble          x,
                       gdouble          y,
                       gpointer         user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
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
            nautilus_list_base_activate_selection (self, FALSE);
        }
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
    else if (button == GDK_BUTTON_MIDDLE && n_press == 1)
    {
        /* Anticipate selection, if necessary, to activate it. */
        select_single_item_if_not_selected (self, cell);
        nautilus_list_base_activate_selection (self, TRUE);
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    if (priv->activate_on_release)
    {
        NautilusViewModel *model;
        guint i = nautilus_view_cell_get_position (cell);

        model = nautilus_list_base_get_model (self);

        /* Anticipate selection, enforcing single selection of target item. */
        gtk_selection_model_select_item (GTK_SELECTION_MODEL (model), i, TRUE);

        nautilus_list_base_activate_selection (self, FALSE);
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
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
        gtk_selection_model_unselect_all (GTK_SELECTION_MODEL (priv->model));
    }

    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    if (button == GDK_BUTTON_SECONDARY)
    {
        g_signal_emit (self, signals[POPUP_BACKGROUND_CONTEXT_MENU], 0, x, y);
    }
}

static void
on_item_longpress_pressed (GtkGestureLongPress *gesture,
                           gdouble              x,
                           gdouble              y,
                           gpointer             user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);

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
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    gtk_selection_model_unselect_all (GTK_SELECTION_MODEL (priv->model));

    g_signal_emit (self, signals[POPUP_BACKGROUND_CONTEXT_MENU], 0, x, y);
}

static GdkContentProvider *
on_item_drag_prepare (GtkDragSource *source,
                      double         x,
                      double         y,
                      gpointer       user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GtkBitset) selection = NULL;
    g_autolist (NautilusFile) selected_files = NULL;
    g_autoslist (GFile) file_list = NULL;
    g_autoptr (GdkPaintable) paintable = NULL;
    GdkDragAction actions;
    gint scale_factor;
    GtkBitsetIter iter;
    guint i;

    if (priv->dnd_disabled)
    {
        return NULL;
    }

    /* Anticipate selection, if necessary, for dragging the clicked item. */
    select_single_item_if_not_selected (self, cell);

    selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    g_return_val_if_fail (!gtk_bitset_is_empty (selection), NULL);

    gtk_gesture_set_state (GTK_GESTURE (source), GTK_EVENT_SEQUENCE_CLAIMED);

    actions = GDK_ACTION_ALL | GDK_ACTION_ASK;

    for (gtk_bitset_iter_init_last (&iter, selection, &i);
         gtk_bitset_iter_is_valid (&iter);
         gtk_bitset_iter_previous (&iter, &i))
    {
        g_autoptr (NautilusViewItem) item = get_view_item (G_LIST_MODEL (priv->model), i);
        NautilusFile *file = nautilus_view_item_get_file (item);

        selected_files = g_list_prepend (selected_files, g_object_ref (file));

        /* Convert to GTK_TYPE_FILE_LIST, which is assumed to be a GSList<GFile>. */
        file_list = g_slist_prepend (file_list, nautilus_file_get_activation_location (file));

        if (!nautilus_file_can_delete (file))
        {
            actions &= ~GDK_ACTION_MOVE;
        }
    }

    gtk_drag_source_set_actions (source, actions);

    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    paintable = get_paintable_for_drag_selection (selected_files, scale_factor);

    gtk_drag_source_set_icon (source, paintable, 0, 0);

    return gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST, file_list);
}

static gboolean
hover_timer (gpointer user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);

    priv->hover_timer_id = 0;

    if (priv->drag_item_action == 0)
    {
        /* If we aren't able to dropped don't change the location. This stops
         * drops onto themselves, and another unnecessary drops. */
        return G_SOURCE_REMOVE;
    }

    NautilusFile *file = nautilus_view_item_get_file (item);

    if (file == priv->directory_as_file ||
        !nautilus_file_is_directory (file) ||
        !g_settings_get_boolean (nautilus_preferences,
                                 NAUTILUS_PREFERENCES_OPEN_FOLDER_ON_DND_HOVER))
    {
        return G_SOURCE_REMOVE;
    }

    NautilusViewModel *model = nautilus_list_base_get_model (self);
    guint i = nautilus_view_cell_get_position (cell);

    gtk_selection_model_select_item (GTK_SELECTION_MODEL (model), i, TRUE);
    nautilus_list_base_activate_selection (self, FALSE);

    return G_SOURCE_REMOVE;
}

static void
on_item_drag_hover_enter (GtkDropControllerMotion *controller,
                          gdouble                  x,
                          gdouble                  y,
                          gpointer                 user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    priv->hover_start_point.x = x;
    priv->hover_start_point.y = y;
}

static void
on_item_drag_hover_leave (GtkDropControllerMotion *controller,
                          gpointer                 user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = NULL;
    const GValue *value;
    g_autoptr (NautilusFile) dest_file = NULL;

    /* Reset action cache. */
    priv->drag_item_action = 0;

    if (priv->dnd_disabled)
    {
        gtk_drop_target_reject (target);
        return 0;
    }

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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);
    GdkDragAction actions;
    g_autoptr (GFile) target_location = nautilus_file_get_location (nautilus_view_item_get_file (item));
    gboolean accepted = FALSE;

    if (priv->dnd_disabled)
    {
        return FALSE;
    }

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

    g_signal_emit (self, signals[PERFORM_DROP], 0,
                   value, actions, target_location,
                   &accepted);
    return accepted;
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

    if (priv->dnd_disabled)
    {
        gtk_drop_target_reject (target);
        return 0;
    }

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
    gboolean accepted = FALSE;

    if (priv->drag_view_action == 0 || priv->dnd_disabled)
    {
        /* We didn't reject earlier because the view's location may change and,
         * as a result, a drop action might become available. */
        return FALSE;
    }

    actions = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));
    target_location = nautilus_file_get_location (priv->directory_as_file);

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

    g_signal_emit (self, signals[PERFORM_DROP], 0,
                   value, actions, target_location,
                   &accepted);
    return accepted;
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
    g_object_bind_property (listitem, "position", cell, "position", G_BINDING_SYNC_CREATE);

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

    /* HACK: Fix for https://gitlab.gnome.org/GNOME/nautilus/-/issues/1452 */
    {
        GtkScrolledWindow *content = GTK_SCROLLED_WINDOW (priv->scrolled_window);

        /* If we load a new location while the view is still scrolling due to
         * kinetic deceleration, we get a sudden jump to the same scrolling
         * position as the previous location, as well as residual scrolling
         * movement in the new location.
         *
         * This is both undesirable and unexpected from a user POV, so we want
         * to abort deceleration when switching locations.
         *
         * However, gtk_scrolled_window_cancel_deceleration() is private. So,
         * we make use of an undocumented behavior of ::set_kinetic_scrolling(),
         * which calls ::cancel_deceleration() when set to FALSE.
         */
        gtk_scrolled_window_set_kinetic_scrolling (content, FALSE);
        gtk_scrolled_window_set_kinetic_scrolling (content, TRUE);
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
    g_autoptr (GtkBitset) selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));

    return gtk_bitset_get_minimum (selection);
}

/**
 * Gets widget to point a popover to. E.g. rename popover, selection menu.
 *
 * If multiple items are selected, we pick the first one.
 *
 * (Ideally it would have been the currently focused item, if it is part of the
 * selection. Sadly, that's not currently possible, because GTK doesn't let us
 * know the focus position: https://gitlab.gnome.org/GNOME/gtk/-/issues/2891 )
 **/
GtkWidget *
nautilus_list_base_get_selected_item_ui (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = NULL;
    guint i = get_first_selected_item (self);

    g_return_val_if_fail (i != G_MAXUINT, NULL);

    /* Make sure the whole item is visible for the popover to point to. */
    nautilus_list_base_scroll_to_item (self, i);

    item = get_view_item (G_LIST_MODEL (priv->model), i);
    return nautilus_view_item_get_item_ui (item);
}

static NautilusViewItem *
default_get_backing_item (NautilusListBase *self)
{
    return NULL;
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
    g_clear_object (&priv->model);
    g_clear_handle_id (&priv->hover_timer_id, g_source_remove);
    g_clear_handle_id (&priv->scroll_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_list_base_parent_class)->dispose (object);
}

static void
nautilus_list_base_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_list_base_parent_class)->finalize (object);
}

static void
on_scroll_timeout (gpointer user_data)
{
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    priv->scroll_timeout_id = 0;
    priv->amount_scrolled_for_zoom = 0;
}

/* handle Ctrl+Scroll, which will cause a zoom-in/out */
static gboolean
on_scroll (GtkEventControllerScroll *scroll,
           gdouble                   dx,
           gdouble                   dy,
           gpointer                  user_data)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (user_data);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GdkModifierType state;

    state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

    if (state & GDK_CONTROL_MASK)
    {
        if (priv->scroll_timeout_id == 0)
        {
            priv->scroll_timeout_id = g_timeout_add_once (SCROLL_TO_ZOOM_INTERVAL,
                                                          on_scroll_timeout, self);
        }

        priv->amount_scrolled_for_zoom += dy;
        if (priv->amount_scrolled_for_zoom <= -1)
        {
            gtk_widget_activate_action (GTK_WIDGET (self), "view.zoom-in", NULL);
            priv->amount_scrolled_for_zoom = 0;
        }
        else if (priv->amount_scrolled_for_zoom >= 1)
        {
            gtk_widget_activate_action (GTK_WIDGET (self), "view.zoom-out", NULL);
            priv->amount_scrolled_for_zoom = 0;
        }

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
nautilus_list_base_focus (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (widget);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GtkBitset) selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    gboolean no_selection = gtk_bitset_is_empty (selection);
    gboolean handled;

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

static gboolean
nautilus_list_base_grab_focus (GtkWidget *widget)
{
    /* focus the child of the scrolled window if it exists */
    NautilusListBase *self = NAUTILUS_LIST_BASE (widget);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkWidget *child = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (priv->scrolled_window));

    if (child != NULL)
    {
        return gtk_widget_grab_focus (child);
    }

    return GTK_WIDGET_CLASS (nautilus_list_base_parent_class)->grab_focus (widget);
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

        case PROP_MODEL:
        {
            g_value_set_object (value, nautilus_list_base_get_model (self));
        }
        break;

        case PROP_SORT_STATE:
        {
            g_value_take_variant (value, nautilus_list_base_get_sort_state (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_list_base_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (object);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_MODEL:
        {
            g_set_object (&priv->model, g_value_get_object (value));
        }
        break;

        case PROP_SORT_STATE:
        {
            nautilus_list_base_set_sort_state (self, g_value_get_variant (value));
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

    object_class->dispose = nautilus_list_base_dispose;
    object_class->finalize = nautilus_list_base_finalize;
    object_class->get_property = nautilus_list_base_get_property;
    object_class->set_property = nautilus_list_base_set_property;

    widget_class->focus = nautilus_list_base_focus;
    widget_class->grab_focus = nautilus_list_base_grab_focus;

    klass->get_backing_item = default_get_backing_item;
    klass->preview_selection_event = default_preview_selection_event;
    klass->setup_directory = base_setup_directory;

    properties[PROP_ICON_SIZE] = g_param_spec_uint ("icon-size",
                                                    "", "",
                                                    NAUTILUS_LIST_ICON_SIZE_SMALL,
                                                    NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE,
                                                    NAUTILUS_GRID_ICON_SIZE_LARGE,
                                                    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    properties[PROP_MODEL] = g_param_spec_object ("model", NULL, NULL,
                                                  NAUTILUS_TYPE_VIEW_MODEL,
                                                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SORT_STATE] = g_param_spec_variant ("sort-state", NULL, NULL,
                                                        G_VARIANT_TYPE ("(sb)"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[ACTIVATE_SELECTION] = g_signal_new ("activate-selection",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__FLAGS,
                                                G_TYPE_NONE, 1,
                                                NAUTILUS_TYPE_OPEN_FLAGS);
    signals[PERFORM_DROP] = g_signal_new ("perform-drop",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL,
                                          NULL,
                                          G_TYPE_BOOLEAN, 3,
                                          G_TYPE_VALUE, GDK_TYPE_DRAG_ACTION, G_TYPE_FILE);
    signals[POPUP_BACKGROUND_CONTEXT_MENU] = g_signal_new ("popup-background-context-menu",
                                                           G_TYPE_FROM_CLASS (klass),
                                                           G_SIGNAL_RUN_FIRST,
                                                           G_STRUCT_OFFSET (NautilusListBaseClass, popup_background_context_menu),
                                                           NULL, NULL,
                                                           NULL,
                                                           G_TYPE_NONE, 2,
                                                           G_TYPE_DOUBLE, G_TYPE_DOUBLE);
    signals[POPUP_SELECTION_CONTEXT_MENU] = g_signal_new ("popup-selection-context-menu",
                                                          G_TYPE_FROM_CLASS (klass),
                                                          G_SIGNAL_RUN_LAST,
                                                          0,
                                                          NULL, NULL,
                                                          NULL,
                                                          G_TYPE_NONE, 3,
                                                          G_TYPE_DOUBLE, G_TYPE_DOUBLE,
                                                          GTK_TYPE_WIDGET);
}

static void
nautilus_list_base_init (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkEventController *controller;

    priv->scrolled_window = gtk_scrolled_window_new ();
    priv->overlay = gtk_overlay_new ();

    gtk_overlay_set_child (GTK_OVERLAY (priv->overlay), priv->scrolled_window);
    adw_bin_set_child (ADW_BIN (self), priv->overlay);

    controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_widget_add_controller (priv->scrolled_window, controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
    g_signal_connect (controller, "scroll", G_CALLBACK (on_scroll), self);

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

GtkWidget *
nautilus_list_base_get_scrolled_window (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    return priv->scrolled_window;
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

void
nautilus_list_base_setup_background_longpress (NautilusListBase *self,
                                               GtkWidget        *child)
{
    GtkEventController *controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());

    gtk_widget_add_controller (GTK_WIDGET (child), controller);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_view_longpress_pressed), self);
}

void
nautilus_list_base_disable_dnd (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    priv->dnd_disabled = TRUE;
}

/**
 * nautilus_list_base_get_backing_item:
 *
 * Get a view item representing the subdirectory which, based on user action,
 * should be used in place the view directory when performing file operations
 * such as create new folders, paste from the clipboard, etc.
 *
 * If the view directory should be used, NULL is returned instead.
 *
 * Returns: (transfer full) (nullable): The subdirectory backing file operations.
 */
NautilusViewItem *
nautilus_list_base_get_backing_item (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_backing_item (self);
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

void
nautilus_list_base_set_zoom_level (NautilusListBase *self,
                                   int               new_zoom_level)
{
    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->set_zoom_level (self, new_zoom_level);
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
