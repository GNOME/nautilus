/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-list-base-private.h"

#include "nautilus-clipboard.h"
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

    GList *cut_files;

    guint scroll_to_file_handle_id;
    guint prioritize_thumbnailing_handle_id;
    GtkAdjustment *vadjustment;

    gboolean single_click_mode;
    gboolean activate_on_release;
    gboolean deny_background_click;

    GdkDragAction drag_item_action;
    GdkDragAction drag_view_action;
    guint hover_timer_id;
    GtkDropTarget *view_drop_target;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusListBase, nautilus_list_base, NAUTILUS_TYPE_FILES_VIEW)

typedef struct
{
    const NautilusFileSortType sort_type;
    const gchar *metadata_name;
} SortConstants;

static const SortConstants sorts_constants[] =
{
    {
        NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
        "name",
    },
    {
        NAUTILUS_FILE_SORT_BY_SIZE,
        "size",
    },
    {
        NAUTILUS_FILE_SORT_BY_TYPE,
        "type",
    },
    {
        NAUTILUS_FILE_SORT_BY_MTIME,
        "modification date",
    },
    {
        NAUTILUS_FILE_SORT_BY_ATIME,
        "access date",
    },
    {
        NAUTILUS_FILE_SORT_BY_BTIME,
        "creation date",
    },
    {
        NAUTILUS_FILE_SORT_BY_TRASHED_TIME,
        "trashed",
    },
    {
        NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE,
        "search_relevance",
    },
    {
        NAUTILUS_FILE_SORT_BY_RECENCY,
        "recency",
    },
};

static inline NautilusViewItem *
get_view_item (GListModel *model,
               guint       position)
{
    return NAUTILUS_VIEW_ITEM (g_list_model_get_item (model, position));
}

static const SortConstants *
get_sorts_constants_from_sort_type (NautilusFileSortType sort_type)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (sort_type == sorts_constants[i].sort_type)
        {
            return &sorts_constants[i];
        }
    }

    return &sorts_constants[0];
}

static const SortConstants *
get_sorts_constants_from_metadata_text (const char *metadata_name)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (g_strcmp0 (sorts_constants[i].metadata_name, metadata_name) == 0)
        {
            return &sorts_constants[i];
        }
    }

    return &sorts_constants[0];
}

const NautilusFileSortType
get_sorts_type_from_metadata_text (const char *metadata_name)
{
    return get_sorts_constants_from_metadata_text (metadata_name)->sort_type;
}

static const SortConstants *
get_default_sort_order (NautilusFile *file,
                        gboolean     *reversed)
{
    NautilusFileSortType sort_type;

    sort_type = nautilus_file_get_default_sort_type (file, reversed);

    return get_sorts_constants_from_sort_type (sort_type);
}

static const SortConstants *
get_directory_sort_by (NautilusFile *file,
                       gboolean     *reversed)
{
    const SortConstants *default_sort;
    g_autofree char *sort_by = NULL;

    default_sort = get_default_sort_order (file, reversed);
    g_return_val_if_fail (default_sort != NULL, NULL);

    sort_by = nautilus_file_get_metadata (file,
                                          NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
                                          default_sort->metadata_name);

    *reversed = nautilus_file_get_boolean_metadata (file,
                                                    NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                                    *reversed);

    return get_sorts_constants_from_metadata_text (sort_by);
}

void
set_directory_sort_metadata (NautilusFile *file,
                             const gchar  *metadata_name,
                             gboolean      reversed)
{
    const SortConstants *default_sort;
    gboolean default_reversed;

    default_sort = get_default_sort_order (file, &default_reversed);

    nautilus_file_set_metadata (file,
                                NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
                                default_sort->metadata_name,
                                metadata_name);
    nautilus_file_set_boolean_metadata (file,
                                        NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                        default_reversed,
                                        reversed);
}

static void
update_sort_order_from_metadata_and_preferences (NautilusListBase *self)
{
    const SortConstants *default_directory_sort;
    GActionGroup *view_action_group;
    gboolean reversed;

    default_directory_sort = get_directory_sort_by (nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self)),
                                                    &reversed);
    view_action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_group_change_action_state (view_action_group,
                                        "sort",
                                        g_variant_new ("(sb)",
                                                       default_directory_sort->metadata_name,
                                                       reversed));
}

void
nautilus_list_base_set_icon_size (NautilusListBase *self,
                                  gint              icon_size)
{
    GListModel *model;
    guint n_items;

    model = G_LIST_MODEL (nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self)));

    n_items = g_list_model_get_n_items (model);
    for (guint i = 0; i < n_items; i++)
    {
        g_autoptr (NautilusViewItem) current_item = NULL;

        current_item = get_view_item (model, i);
        nautilus_view_item_set_icon_size (current_item, icon_size);
    }
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

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    position = nautilus_view_model_get_index (model, item);
    if (!gtk_selection_model_is_selected (GTK_SELECTION_MODEL (model), position))
    {
        gtk_selection_model_select_item (GTK_SELECTION_MODEL (model), position, TRUE);
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
    NautilusViewItem *item;
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
     * during a drag and drop.  Disable rubberband on item press and enable
     * rubberbnad on item release/stop.  The permanent solution for this is
     * https://gitlab.gnome.org/GNOME/gtk/-/merge_requests/4831 */

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
    NautilusListBase *self = NAUTILUS_LIST_BASE (nautilus_view_cell_get_view (cell));
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
        NautilusViewItem *item = nautilus_view_cell_get_item (cell);
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
    NautilusListBase *self = NAUTILUS_LIST_BASE (nautilus_view_cell_get_view (cell));
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    if (priv->activate_on_release)
    {
        NautilusViewModel *model;
        NautilusViewItem *item;
        guint i;

        model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
        item = nautilus_view_cell_get_item (cell);
        g_return_if_fail (item != NULL);
        i = nautilus_view_model_get_index (model, item);

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
    NautilusListBase *self = NAUTILUS_LIST_BASE (nautilus_view_cell_get_view (cell));
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

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
    NautilusListBase *self = NAUTILUS_LIST_BASE (nautilus_view_cell_get_view (cell));

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
    NautilusListBase *self = NAUTILUS_LIST_BASE (nautilus_view_cell_get_view (cell));
    g_autolist (NautilusFile) selection = NULL;
    g_autoslist (GFile) file_list = NULL;
    g_autoptr (GdkPaintable) paintable = NULL;
    GdkDragAction actions;
    gint scale_factor;

    /* Anticipate selection, if necessary, for dragging the clicked item. */
    select_single_item_if_not_selected (self, nautilus_view_cell_get_item (cell));

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    g_return_val_if_fail (selection != NULL, NULL);

    gtk_gesture_set_state (GTK_GESTURE (source), GTK_EVENT_SEQUENCE_CLAIMED);

    /* Convert to GTK_TYPE_FILE_LIST, which is assumed to be a GSList<GFile>. */
    for (GList *l = selection; l != NULL; l = l->next)
    {
        file_list = g_slist_prepend (file_list, nautilus_file_get_location (l->data));
    }
    file_list = g_slist_reverse (file_list);

    actions = GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK | GDK_ACTION_MOVE;
    gtk_drag_source_set_actions (source, actions);

    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    paintable = get_paintable_for_drag_selection (selection, scale_factor);
    gtk_drag_source_set_icon (source, paintable, 0, 0);

    return gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST, file_list);
}

static gboolean
hover_timer (gpointer user_data)
{
    NautilusViewCell *cell = user_data;
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    NautilusViewItem *item = nautilus_view_cell_get_item (cell);
    g_autofree gchar *uri = NULL;

    priv->hover_timer_id = 0;

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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    if (priv->hover_timer_id == 0)
    {
        priv->hover_timer_id = g_timeout_add (HOVER_TIMEOUT, hover_timer, cell);
    }
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

static GdkDragAction
get_preferred_action (NautilusFile *target_file,
                      const GValue *value)
{
    GdkDragAction action = 0;

    if (value == NULL)
    {
        action = nautilus_dnd_get_prefered_action (target_file, NULL);
    }
    if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *source_file_list = g_value_get_boxed (value);
        action = nautilus_dnd_get_prefered_action (target_file, source_file_list->data);
    }

    return action;
}

static void
real_perform_drop (NautilusListBase *self,
                   const GValue     *value,
                   GdkDragAction     action,
                   GFile            *target_location)
{
    if (!gdk_drag_action_is_unique (action))
    {
        /* TODO: Implement */
    }
    else if (G_VALUE_HOLDS (value, G_TYPE_STRING))
    {
        /* TODO: Implement */
    }
    else if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    {
        GSList *source_file_list = g_value_get_boxed (value);
        GList *source_uri_list = NULL;
        g_autofree gchar *target_uri = NULL;

        for (GSList *l = source_file_list; l != NULL; l = l->next)
        {
            source_uri_list = g_list_prepend (source_uri_list, g_file_get_uri (l->data));
        }
        source_uri_list = g_list_reverse (source_uri_list);

        target_uri = g_file_get_uri (target_location);
        nautilus_files_view_drop_proxy_received_uris (NAUTILUS_FILES_VIEW (self),
                                                      source_uri_list,
                                                      target_uri,
                                                      action);
        g_list_free_full (source_uri_list, g_free);
    }
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
    NautilusViewItem *item;
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
    NautilusListBase *self = nautilus_view_cell_get_view (cell);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    const GValue *value;
    NautilusViewItem *item;

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
    NautilusViewItem *item = nautilus_view_cell_get_item (cell);

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
    NautilusViewItem *item = nautilus_view_cell_get_item (cell);
    GdkDragAction actions;
    GFile *target_location;

    actions = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));
    target_location = nautilus_file_get_location (nautilus_view_item_get_file (item));

    real_perform_drop (self, value, actions, target_location);

    return TRUE;
}

static GdkDragAction
on_view_drag_enter (GtkDropTarget *target,
                    double         x,
                    double         y,
                    gpointer       user_data)
{
    NautilusListBase *self = user_data;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    NautilusFile *dest_file;
    const GValue *value;

    dest_file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));
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
    NautilusFile *dest_file;

    value = gtk_drop_target_get_value (target);
    if (value == NULL)
    {
        return;
    }

    dest_file = nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self));
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
    GFile *target_location;

    if (priv->drag_view_action == 0)
    {
        /* We didn't reject earlier because the view's location may change and,
         * as a result, a drop action might become available. */
        return FALSE;
    }

    actions = gdk_drop_get_actions (gtk_drop_target_get_current_drop (target));
    target_location = nautilus_view_get_location (NAUTILUS_VIEW (self));

    real_perform_drop (self, value, actions, target_location);

    return TRUE;
}

void
setup_cell_common (GtkListItem      *listitem,
                   NautilusViewCell *cell)
{
    GtkEventController *controller;
    GtkDropTarget *drop_target;

    g_object_bind_property (listitem, "item",
                            cell, "item",
                            G_BINDING_SYNC_CREATE);
    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));

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
    drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_drop_target_set_preload (drop_target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (drop_target, (GType[1]) { GDK_TYPE_FILE_LIST }, 1);
    g_signal_connect (drop_target, "enter", G_CALLBACK (on_item_drag_enter), cell);
    g_signal_connect (drop_target, "notify::value", G_CALLBACK (on_item_drag_value_notify), cell);
    g_signal_connect (drop_target, "leave", G_CALLBACK (on_item_drag_leave), cell);
    g_signal_connect (drop_target, "motion", G_CALLBACK (on_item_drag_motion), cell);
    g_signal_connect (drop_target, "drop", G_CALLBACK (on_item_drop), cell);
    gtk_widget_add_controller (GTK_WIDGET (cell), GTK_EVENT_CONTROLLER (drop_target));

    controller = gtk_drop_controller_motion_new ();
    gtk_widget_add_controller (GTK_WIDGET (cell), controller);
    g_signal_connect (controller, "enter", G_CALLBACK (on_item_drag_hover_enter), cell);
    g_signal_connect (controller, "leave", G_CALLBACK (on_item_drag_hover_leave), cell);
}

static void
nautilus_list_base_scroll_to_item (NautilusListBase *self,
                                   guint             position)
{
    NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->scroll_to_item (self, position);
}

static guint
nautilus_list_base_get_icon_size (NautilusListBase *self)
{
    return NAUTILUS_LIST_BASE_CLASS (G_OBJECT_GET_CLASS (self))->get_icon_size (self);
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
real_begin_loading (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    /* Temporary workaround */
    rubberband_set_state (self, TRUE);

    /*TODO move this to the files view class begin_loading and hook up? */


    /* TODO: This calls sort once, and update_context_menus calls update_actions
     * which calls the action again
     */
    update_sort_order_from_metadata_and_preferences (NAUTILUS_LIST_BASE (files_view));

    /* We could have changed to the trash directory or to searching, and then
     * we need to update the menus */
    nautilus_files_view_update_context_menus (files_view);
    nautilus_files_view_update_toolbar_menus (files_view);

    /* When double clicking on an item this deny_background_click can persist
     * because the new view interrupts the gesture sequence, so lets reset it.*/
    priv->deny_background_click = FALSE;

    /* When DnD is used to navigate between directories, the normal callbacks
     * are ignored. Update DnD variables here upon navigating to a directory*/
    priv->drag_view_action = get_preferred_action (nautilus_files_view_get_directory_as_file (files_view),
                                                   gtk_drop_target_get_value (priv->view_drop_target));
    priv->drag_item_action = 0;
}

static void
real_clear (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    nautilus_view_model_remove_all_items (priv->model);
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

static void
real_click_policy_changed (NautilusFilesView *files_view)
{
    set_click_mode_from_settings (NAUTILUS_LIST_BASE (files_view));
}

static void
real_file_changed (NautilusFilesView *files_view,
                   NautilusFile      *file,
                   NautilusDirectory *directory)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    NautilusViewItem *item;

    item = nautilus_view_model_get_item_from_file (priv->model, file);
    nautilus_view_item_file_changed (item);
}

static GList *
real_get_selection (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GtkSelectionFilterModel) selection = NULL;
    guint n_selected;
    GList *selected_files = NULL;

    selection = gtk_selection_filter_model_new (GTK_SELECTION_MODEL (priv->model));
    n_selected = g_list_model_get_n_items (G_LIST_MODEL (selection));
    for (guint i = 0; i < n_selected; i++)
    {
        g_autoptr (NautilusViewItem) item = NULL;

        item = get_view_item (G_LIST_MODEL (selection), i);
        selected_files = g_list_prepend (selected_files,
                                         g_object_ref (nautilus_view_item_get_file (item)));
    }

    return selected_files;
}

static gboolean
real_is_empty (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    return g_list_model_get_n_items (G_LIST_MODEL (priv->model)) == 0;
}

static void
real_end_file_changes (NautilusFilesView *files_view)
{
}

static void
real_remove_file (NautilusFilesView *files_view,
                  NautilusFile      *file,
                  NautilusDirectory *directory)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    NautilusViewItem *item;

    item = nautilus_view_model_get_item_from_file (priv->model, file);
    if (item != NULL)
    {
        nautilus_view_model_remove_item (priv->model, item);
    }
}

static GQueue *
convert_glist_to_queue (GList *list)
{
    GList *l;
    GQueue *queue;

    queue = g_queue_new ();
    for (l = list; l != NULL; l = l->next)
    {
        g_queue_push_tail (queue, l->data);
    }

    return queue;
}

static GQueue *
convert_files_to_items (NautilusListBase *self,
                        GQueue           *files)
{
    GList *l;
    GQueue *models;

    models = g_queue_new ();
    for (l = g_queue_peek_head_link (files); l != NULL; l = l->next)
    {
        NautilusViewItem *item;

        item = nautilus_view_item_new (NAUTILUS_FILE (l->data),
                                       nautilus_list_base_get_icon_size (self));
        g_queue_push_tail (models, item);
    }

    return models;
}

static void
real_set_selection (NautilusFilesView *files_view,
                    GList             *selection)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GQueue) selection_files = NULL;
    g_autoptr (GQueue) selection_items = NULL;
    g_autoptr (GtkBitset) update_set = NULL;
    g_autoptr (GtkBitset) selection_set = NULL;

    update_set = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (priv->model));
    selection_set = gtk_bitset_new_empty ();

    /* Convert file list into set of model indices */
    selection_files = convert_glist_to_queue (selection);
    selection_items = nautilus_view_model_get_items_from_files (priv->model, selection_files);
    for (GList *l = g_queue_peek_head_link (selection_items); l != NULL; l = l->next)
    {
        gtk_bitset_add (selection_set,
                        nautilus_view_model_get_index (priv->model, l->data));
    }

    /* Set focus on the first selected row. */
    if (!g_queue_is_empty (selection_items))
    {
        NautilusViewItem *item = g_queue_peek_head (selection_items);
        GtkWidget *item_widget = nautilus_view_item_get_item_ui (item);
        GtkWidget *parent = gtk_widget_get_parent (item_widget);

        if (!gtk_widget_grab_focus (parent))
        {
            /* In GtkColumnView, the parent is a cell; its parent is the row. */
            gtk_widget_grab_focus (gtk_widget_get_parent (parent));
        }

        /* HACK: Grabbing focus is not enough for the listbase item tracker to
         * acknowledge it. So, poke the internal actions to fix the bug reported
         * in https://gitlab.gnome.org/GNOME/nautilus/-/issues/2294 */
        gtk_widget_activate_action (item_widget,
                                    "list.select-item",
                                    "(ubb)",
                                    nautilus_view_model_get_index (priv->model, item),
                                    FALSE, FALSE);
    }

    gtk_bitset_union (update_set, selection_set);
    gtk_selection_model_set_selection (GTK_SELECTION_MODEL (priv->model),
                                       selection_set,
                                       update_set);
}

static void
real_select_all (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    gtk_selection_model_select_all (GTK_SELECTION_MODEL (priv->model));
}

static void
real_invert_selection (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (priv->model);
    g_autoptr (GtkBitset) selected = NULL;
    g_autoptr (GtkBitset) all = NULL;
    g_autoptr (GtkBitset) new_selected = NULL;

    selected = gtk_selection_model_get_selection (selection_model);

    /* We are going to flip the selection state of every item in the model. */
    all = gtk_bitset_new_range (0, g_list_model_get_n_items (G_LIST_MODEL (priv->model)));

    /* The new selection is all items minus the ones currently selected. */
    new_selected = gtk_bitset_copy (all);
    gtk_bitset_subtract (new_selected, selected);

    gtk_selection_model_set_selection (selection_model, new_selected, all);
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
    item = nautilus_view_model_get_item_from_file (priv->model, file);

    return nautilus_view_model_get_index (priv->model, item);
}

static void
real_reveal_selection (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);

    nautilus_list_base_scroll_to_item (self, get_first_selected_item (self));
}

static int
real_compare_files (NautilusFilesView *files_view,
                    NautilusFile      *file1,
                    NautilusFile      *file2)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkSorter *sorter;
    g_autoptr (NautilusViewItem) item1 = NULL;
    g_autoptr (NautilusViewItem) item2 = NULL;

    sorter = nautilus_view_model_get_sorter (priv->model);
    if (sorter == NULL)
    {
        return 0;
    }

    /* Generate fake model items for sorter use only. */
    item1 = nautilus_view_item_new (file1, NAUTILUS_GRID_ICON_SIZE_SMALL);
    item2 = nautilus_view_item_new (file2, NAUTILUS_GRID_ICON_SIZE_SMALL);

    return gtk_sorter_compare (sorter, item1, item2);
}

static void
on_clipboard_contents_received (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
    NautilusFilesView *files_view = NAUTILUS_FILES_VIEW (source_object);
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    NautilusClipboard *clip;
    NautilusViewItem *item;

    for (GList *l = priv->cut_files; l != NULL; l = l->next)
    {
        item = nautilus_view_model_get_item_from_file (priv->model, l->data);
        if (item != NULL)
        {
            nautilus_view_item_set_cut (item, FALSE);
        }
    }
    g_clear_list (&priv->cut_files, g_object_unref);

    clip = nautilus_files_view_get_clipboard_finish (files_view, res, NULL);
    if (clip != NULL && nautilus_clipboard_is_cut (clip))
    {
        priv->cut_files = g_list_copy_deep (nautilus_clipboard_peek_files (clip),
                                            (GCopyFunc) g_object_ref,
                                            NULL);
    }

    for (GList *l = priv->cut_files; l != NULL; l = l->next)
    {
        item = nautilus_view_model_get_item_from_file (priv->model, l->data);
        if (item != NULL)
        {
            nautilus_view_item_set_cut (item, TRUE);
        }
    }
}

static void
update_clipboard_status (NautilusFilesView *view)
{
    nautilus_files_view_get_clipboard_async (view,
                                             on_clipboard_contents_received,
                                             NULL);
}

static void
on_clipboard_owner_changed (GdkClipboard *clipboard,
                            gpointer      user_data)
{
    update_clipboard_status (NAUTILUS_FILES_VIEW (user_data));
}

static void
real_end_loading (NautilusFilesView *files_view,
                  gboolean           all_files_seen)
{
    update_clipboard_status (files_view);
}

static guint
get_first_visible_item (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    guint n_items;
    gdouble scrolled_y;
    GtkWidget *view_ui;

    n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->model));
    scrolled_y = gtk_adjustment_get_value (priv->vadjustment);
    view_ui = nautilus_list_base_get_view_ui (self);
    for (guint i = 0; i < n_items; i++)
    {
        g_autoptr (NautilusViewItem) item = NULL;
        GtkWidget *item_ui;

        item = get_view_item (G_LIST_MODEL (priv->model), i);
        item_ui = nautilus_view_item_get_item_ui (item);
        if (item_ui != NULL)
        {
            gdouble y;

            gtk_widget_translate_coordinates (item_ui, view_ui,
                                              0, 0, NULL, &y);
            if (gtk_widget_is_visible (item_ui) && y >= scrolled_y)
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

typedef struct
{
    NautilusListBase *view;
    char *uri;
} ScrollToFileData;

static void
scroll_to_file_data_free (ScrollToFileData *data)
{
    g_free (data->uri);
    g_free (data);
}

static gboolean
scroll_to_file_on_idle (ScrollToFileData *data)
{
    NautilusListBase *self = data->view;
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusFile) file = NULL;
    NautilusViewItem *item;
    guint i;

    file = nautilus_file_get_existing_by_uri (data->uri);
    item = nautilus_view_model_get_item_from_file (priv->model, file);
    i = nautilus_view_model_get_index (priv->model, item);

    nautilus_list_base_scroll_to_item (self, i);

    priv->scroll_to_file_handle_id = 0;
    return G_SOURCE_REMOVE;
}

static void
real_scroll_to_file (NautilusFilesView *files_view,
                     const char        *uri)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    ScrollToFileData *data;
    guint handle_id;

    data = g_new (ScrollToFileData, 1);
    data->view = self;
    data->uri = g_strdup (uri);
    handle_id = g_idle_add_full (G_PRIORITY_LOW,
                                 (GSourceFunc) scroll_to_file_on_idle,
                                 data,
                                 (GDestroyNotify) scroll_to_file_data_free);
    priv->scroll_to_file_handle_id = handle_id;
}

static void
real_add_files (NautilusFilesView *files_view,
                GList             *files)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GQueue) files_queue = NULL;
    g_autoptr (GQueue) items = NULL;
    gdouble adjustment_value;

    files_queue = convert_glist_to_queue (files);
    items = convert_files_to_items (self, files_queue);
    nautilus_view_model_add_items (priv->model, items);

    /* GtkListBase anchoring doesn't cope well with our lazy loading.
     * Assuming that GtkListBase|list.scroll-to-item resets the anchor to 0, use
     * that as a workaround to prevent scrolling while we are at the top. */
    adjustment_value = gtk_adjustment_get_value (priv->vadjustment);
    if (G_APPROX_VALUE (adjustment_value, 0.0, DBL_EPSILON))
    {
        nautilus_list_base_scroll_to_item (self, 0);
    }
}

static void
real_select_first (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;
    g_autoptr (GList) selection = NULL;

    item = get_view_item (G_LIST_MODEL (priv->model), 0);
    if (item == NULL)
    {
        return;
    }
    file = nautilus_view_item_get_file (item);
    selection = g_list_prepend (selection, file);
    nautilus_view_set_selection (NAUTILUS_VIEW (files_view), selection);
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

static GdkRectangle *
real_reveal_for_selection_context_menu (NautilusFilesView *files_view)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    g_autoptr (GtkSelectionFilterModel) selection = NULL;
    guint n_selected;
    GtkWidget *focus_child;
    guint i;
    GtkWidget *item_ui;

    selection = gtk_selection_filter_model_new (GTK_SELECTION_MODEL (priv->model));
    n_selected = g_list_model_get_n_items (G_LIST_MODEL (selection));
    g_return_val_if_fail (n_selected > 0, NULL);

    /* Get the focused item_ui, if selected.
     * Otherwise, get the selected item_ui which is sorted the lowest.*/
    focus_child = gtk_widget_get_focus_child (nautilus_list_base_get_view_ui (self));
    for (i = 0; i < n_selected; i++)
    {
        g_autoptr (NautilusViewItem) item = NULL;

        item = get_view_item (G_LIST_MODEL (selection), i);
        item_ui = nautilus_view_item_get_item_ui (item);
        if (item_ui != NULL && gtk_widget_get_parent (item_ui) == focus_child)
        {
            break;
        }
    }
    nautilus_list_base_scroll_to_item (self, i);

    return get_rectangle_for_item_ui (self, item_ui);
}

static void
real_preview_selection_event (NautilusFilesView *files_view,
                              GtkDirectionType   direction)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (files_view);
    GtkMovementStep step;
    gint count;
    gboolean handled;

    step = (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN) ?
           GTK_MOVEMENT_DISPLAY_LINES : GTK_MOVEMENT_VISUAL_POSITIONS;
    count = (direction == GTK_DIR_RIGHT || direction == GTK_DIR_DOWN) ?
            1 : -1;

    g_signal_emit_by_name (nautilus_list_base_get_view_ui (self),
                           "move-cursor", step, count, &handled);
}

static void
default_sort_order_changed_callback (NautilusListBase *self)
{
    update_sort_order_from_metadata_and_preferences (self);
}

static void
nautilus_list_base_dispose (GObject *object)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (object);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    g_clear_handle_id (&priv->scroll_to_file_handle_id, g_source_remove);
    g_clear_handle_id (&priv->prioritize_thumbnailing_handle_id, g_source_remove);
    g_clear_handle_id (&priv->hover_timer_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_list_base_parent_class)->dispose (object);
}

static void
nautilus_list_base_finalize (GObject *object)
{
    NautilusListBase *self = NAUTILUS_LIST_BASE (object);
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);

    g_clear_list (&priv->cut_files, g_object_unref);

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
    for (gint i = 0; i <= last_index - first_index; i++)
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

static void
nautilus_list_base_class_init (NautilusListBaseClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFilesViewClass *files_view_class = NAUTILUS_FILES_VIEW_CLASS (klass);

    object_class->dispose = nautilus_list_base_dispose;
    object_class->finalize = nautilus_list_base_finalize;

    files_view_class->add_files = real_add_files;
    files_view_class->begin_loading = real_begin_loading;
    files_view_class->clear = real_clear;
    files_view_class->click_policy_changed = real_click_policy_changed;
    files_view_class->file_changed = real_file_changed;
    files_view_class->get_selection = real_get_selection;
    /* TODO: remove this get_selection_for_file_transfer, this doesn't even
     * take into account we could us the view for recursive search :/
     * CanvasView has the same issue. */
    files_view_class->get_selection_for_file_transfer = real_get_selection;
    files_view_class->is_empty = real_is_empty;
    files_view_class->remove_file = real_remove_file;
    files_view_class->select_all = real_select_all;
    files_view_class->set_selection = real_set_selection;
    files_view_class->invert_selection = real_invert_selection;
    files_view_class->compare_files = real_compare_files;
    files_view_class->end_file_changes = real_end_file_changes;
    files_view_class->end_loading = real_end_loading;
    files_view_class->get_first_visible_file = real_get_first_visible_file;
    files_view_class->reveal_selection = real_reveal_selection;
    files_view_class->scroll_to_file = real_scroll_to_file;
    files_view_class->select_first = real_select_first;
    files_view_class->compute_rename_popover_pointing_to = real_compute_rename_popover_pointing_to;
    files_view_class->reveal_for_selection_context_menu = real_reveal_for_selection_context_menu;
    files_view_class->preview_selection_event = real_preview_selection_event;
}

static void
nautilus_list_base_init (NautilusListBase *self)
{
    NautilusListBasePrivate *priv = nautilus_list_base_get_instance_private (self);
    GtkWidget *content_widget;
    GtkAdjustment *vadjustment;

    gtk_widget_add_css_class (GTK_WIDGET (self), "view");

    g_signal_connect_object (nautilus_preferences,
                             "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
                             G_CALLBACK (default_sort_order_changed_callback),
                             self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (nautilus_preferences,
                             "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                             G_CALLBACK (default_sort_order_changed_callback),
                             self,
                             G_CONNECT_SWAPPED);

    /* React to clipboard changes */
    g_signal_connect_object (gdk_display_get_clipboard (gdk_display_get_default ()),
                             "changed",
                             G_CALLBACK (on_clipboard_owner_changed), self, 0);

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (content_widget));

    priv->vadjustment = vadjustment;
    g_signal_connect (vadjustment, "changed", (GCallback) on_vadjustment_changed, self);
    g_signal_connect (vadjustment, "value-changed", (GCallback) on_vadjustment_changed, self);

    priv->model = nautilus_view_model_new ();

    g_signal_connect_object (GTK_SELECTION_MODEL (priv->model),
                             "selection-changed",
                             G_CALLBACK (nautilus_files_view_notify_selection_changed),
                             NAUTILUS_FILES_VIEW (self),
                             G_CONNECT_SWAPPED);

    set_click_mode_from_settings (self);
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
    GtkWidget *view_ui = nautilus_list_base_get_view_ui (self);
    GtkEventController *controller;
    GtkDropTarget *drop_target;

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (view_ui, controller);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_view_click_pressed), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (view_ui, controller);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_view_longpress_pressed), self);

    /* TODO: Implement GDK_ACTION_ASK */
    drop_target = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    gtk_drop_target_set_preload (drop_target, TRUE);
    /* TODO: Implement GDK_TYPE_STRING */
    gtk_drop_target_set_gtypes (drop_target, (GType[1]) { GDK_TYPE_FILE_LIST }, 1);
    g_signal_connect (drop_target, "enter", G_CALLBACK (on_view_drag_enter), self);
    g_signal_connect (drop_target, "notify::value", G_CALLBACK (on_view_drag_value_notify), self);
    g_signal_connect (drop_target, "motion", G_CALLBACK (on_view_drag_motion), self);
    g_signal_connect (drop_target, "drop", G_CALLBACK (on_view_drop), self);
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drop_target));
    priv->view_drop_target = drop_target;
}
