/*
 * Nautilus
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "nautilus-toolbar.h"

#include <glib/gi18n.h>
#include <math.h>

#include "nautilus-application.h"
#include "nautilus-bookmark.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-global-preferences.h"
#include "nautilus-location-entry.h"
#include "nautilus-pathbar.h"
#include "nautilus-progress-info-manager.h"
#include "nautilus-progress-info-widget.h"
#include "nautilus-toolbar-menu-sections.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-window.h"

#define OPERATION_MINIMUM_TIME 2 /*s */
#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 3 /*s */

/* Just design, context at https://gitlab.gnome.org/GNOME/nautilus/issues/548#note_274131 */

typedef enum
{
    NAUTILUS_NAVIGATION_DIRECTION_NONE,
    NAUTILUS_NAVIGATION_DIRECTION_BACK,
    NAUTILUS_NAVIGATION_DIRECTION_FORWARD
} NautilusNavigationDirection;

struct _NautilusToolbar
{
    AdwBin parent_instance;

    NautilusWindow *window;

    GtkWidget *path_bar_container;
    GtkWidget *location_entry_container;
    GtkWidget *search_container;
    GtkWidget *toolbar_switcher;
    GtkWidget *path_bar;
    GtkWidget *location_entry;

    gboolean show_location_entry;
    gboolean location_entry_should_auto_hide;

    guint start_operations_timeout_id;
    guint remove_finished_operations_timeout_id;
    guint operations_button_attention_timeout_id;

    GtkWidget *operations_button;
    GtkWidget *operations_popover;
    GtkWidget *operations_list;
    GListStore *progress_infos_model;
    GtkWidget *operations_revealer;
    GtkWidget *operations_icon;

    GtkWidget *view_toggle_button;
    GtkWidget *view_toggle_icon;
    GtkWidget *view_button;
    GMenuModel *view_menu;

    GtkWidget *app_button;
    GMenuModel *undo_redo_section;

    GtkWidget *forward_button;
    GtkWidget *forward_menu;
    GtkGesture *forward_button_longpress_gesture;
    GtkGesture *forward_button_multi_press_gesture;

    GtkWidget *back_button;
    GtkWidget *back_menu;
    GtkGesture *back_button_longpress_gesture;
    GtkGesture *back_button_multi_press_gesture;

    GtkWidget *search_button;

    GtkWidget *location_entry_close_button;

    NautilusProgressInfoManager *progress_manager;

    /* active slot & bindings */
    NautilusWindowSlot *window_slot;
    GBinding *icon_binding;
    GBinding *search_binding;
    GBinding *tooltip_binding;
};

enum
{
    PROP_WINDOW = 1,
    PROP_SHOW_LOCATION_ENTRY,
    PROP_WINDOW_SLOT,
    PROP_SEARCHING,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusToolbar, nautilus_toolbar, ADW_TYPE_BIN);

static void nautilus_toolbar_set_window_slot_real (NautilusToolbar    *self,
                                                   NautilusWindowSlot *slot);
static void update_operations (NautilusToolbar *self);

static void
toolbar_update_appearance (NautilusToolbar *self)
{
    gboolean show_location_entry;

    show_location_entry = self->show_location_entry ||
                          g_settings_get_boolean (nautilus_preferences,
                                                  NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

    if (self->window_slot != NULL &&
        nautilus_window_slot_get_searching (self->window_slot))
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->toolbar_switcher), "search");
    }
    else if (show_location_entry)
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->toolbar_switcher), "location");
    }
    else
    {
        gtk_stack_set_visible_child_name (GTK_STACK (self->toolbar_switcher), "pathbar");
    }
}

static void
fill_menu (NautilusToolbar *self,
           GMenu           *menu,
           gboolean         back)
{
    guint index;
    GList *list;
    const gchar *name;

    list = back ? nautilus_window_slot_get_back_history (self->window_slot) :
           nautilus_window_slot_get_forward_history (self->window_slot);

    index = 0;
    while (list != NULL)
    {
        g_autoptr (GMenuItem) item = NULL;

        name = nautilus_bookmark_get_name (NAUTILUS_BOOKMARK (list->data));
        item = g_menu_item_new (name, NULL);
        g_menu_item_set_action_and_target (item,
                                           back ? "win.back-n" : "win.forward-n",
                                           "u", index);
        g_menu_append_item (menu, item);

        list = g_list_next (list);
        ++index;
    }
}

static void
show_menu (NautilusToolbar *self,
           GtkWidget       *widget)
{
    g_autoptr (GMenu) menu = NULL;
    NautilusNavigationDirection direction;
    GtkPopover *popover;

    menu = g_menu_new ();

    direction = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                     "nav-direction"));

    switch (direction)
    {
        case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
        {
            fill_menu (self, menu, FALSE);
            popover = GTK_POPOVER (self->forward_menu);
        }
        break;

        case NAUTILUS_NAVIGATION_DIRECTION_BACK:
        {
            fill_menu (self, menu, TRUE);
            popover = GTK_POPOVER (self->back_menu);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    gtk_popover_bind_model (popover, G_MENU_MODEL (menu), NULL);
    gtk_popover_popup (popover);
}

static void
navigation_button_press_cb (GtkGestureMultiPress *gesture,
                            gint                  n_press,
                            gdouble               x,
                            gdouble               y,
                            gpointer              user_data)
{
    NautilusToolbar *self;
    GtkWidget *widget;

    self = NAUTILUS_TOOLBAR (user_data);
    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

    show_menu (self, widget);
}

static void
back_button_longpress_cb (GtkGestureLongPress *gesture,
                          double               x,
                          double               y,
                          gpointer             user_data)
{
    NautilusToolbar *self = user_data;

    show_menu (self, self->back_button);
}

static void
forward_button_longpress_cb (GtkGestureLongPress *gesture,
                             double               x,
                             double               y,
                             gpointer             user_data)
{
    NautilusToolbar *self = user_data;

    show_menu (self, self->forward_button);
}

static gboolean
should_show_progress_info (NautilusProgressInfo *info)
{
    return nautilus_progress_info_get_total_elapsed_time (info) +
           nautilus_progress_info_get_remaining_time (info) > OPERATION_MINIMUM_TIME;
}

static GList *
get_filtered_progress_infos (NautilusToolbar *self)
{
    GList *l;
    GList *filtered_progress_infos;
    GList *progress_infos;

    progress_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    filtered_progress_infos = NULL;

    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (should_show_progress_info (l->data))
        {
            filtered_progress_infos = g_list_append (filtered_progress_infos, l->data);
        }
    }

    return filtered_progress_infos;
}

static gboolean
should_hide_operations_button (NautilusToolbar *self)
{
    GList *progress_infos;
    GList *l;

    progress_infos = get_filtered_progress_infos (self);

    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (nautilus_progress_info_get_total_elapsed_time (l->data) +
            nautilus_progress_info_get_remaining_time (l->data) > OPERATION_MINIMUM_TIME &&
            !nautilus_progress_info_get_is_cancelled (l->data) &&
            !nautilus_progress_info_get_is_finished (l->data))
        {
            return FALSE;
        }
    }

    g_list_free (progress_infos);

    return TRUE;
}

static gboolean
on_remove_finished_operations_timeout (NautilusToolbar *self)
{
    nautilus_progress_info_manager_remove_finished_or_cancelled_infos (self->progress_manager);
    if (should_hide_operations_button (self))
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->operations_revealer),
                                       FALSE);
    }
    else
    {
        update_operations (self);
    }

    self->remove_finished_operations_timeout_id = 0;

    return G_SOURCE_REMOVE;
}

static void
unschedule_remove_finished_operations (NautilusToolbar *self)
{
    if (self->remove_finished_operations_timeout_id != 0)
    {
        g_source_remove (self->remove_finished_operations_timeout_id);
        self->remove_finished_operations_timeout_id = 0;
    }
}

static void
schedule_remove_finished_operations (NautilusToolbar *self)
{
    if (self->remove_finished_operations_timeout_id == 0)
    {
        self->remove_finished_operations_timeout_id =
            g_timeout_add_seconds (REMOVE_FINISHED_OPERATIONS_TIEMOUT,
                                   (GSourceFunc) on_remove_finished_operations_timeout,
                                   self);
    }
}

static void
remove_operations_button_attention_style (NautilusToolbar *self)
{
    GtkStyleContext *style_context;

    style_context = gtk_widget_get_style_context (self->operations_button);
    gtk_style_context_remove_class (style_context,
                                    "nautilus-operations-button-needs-attention");
}

static gboolean
on_remove_operations_button_attention_style_timeout (NautilusToolbar *self)
{
    remove_operations_button_attention_style (self);
    self->operations_button_attention_timeout_id = 0;

    return G_SOURCE_REMOVE;
}

static void
unschedule_operations_button_attention_style (NautilusToolbar *self)
{
    if (self->operations_button_attention_timeout_id != 0)
    {
        g_source_remove (self->operations_button_attention_timeout_id);
        self->operations_button_attention_timeout_id = 0;
    }
}

static void
add_operations_button_attention_style (NautilusToolbar *self)
{
    GtkStyleContext *style_context;

    style_context = gtk_widget_get_style_context (self->operations_button);

    unschedule_operations_button_attention_style (self);
    remove_operations_button_attention_style (self);

    gtk_style_context_add_class (style_context,
                                 "nautilus-operations-button-needs-attention");
    self->operations_button_attention_timeout_id = g_timeout_add (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                                                                  (GSourceFunc) on_remove_operations_button_attention_style_timeout,
                                                                  self);
}

static void
on_progress_info_cancelled (NautilusToolbar *self)
{
    /* Update the pie chart progress */
    gtk_widget_queue_draw (self->operations_icon);

    if (!nautilus_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }
}

static void
on_progress_info_progress_changed (NautilusToolbar *self)
{
    /* Update the pie chart progress */
    gtk_widget_queue_draw (self->operations_icon);
}

static void
on_progress_info_finished (NautilusToolbar      *self,
                           NautilusProgressInfo *info)
{
    gchar *main_label;
    GFile *folder_to_open;

    /* Update the pie chart progress */
    gtk_widget_queue_draw (self->operations_icon);

    if (!nautilus_progress_manager_has_viewers (self->progress_manager))
    {
        schedule_remove_finished_operations (self);
    }

    folder_to_open = nautilus_progress_info_get_destination (info);
    /* If destination is null, don't show a notification. This happens when the
     * operation is a trash operation, which we already show a diferent kind of
     * notification */
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->operations_button)) &&
        folder_to_open != NULL)
    {
        add_operations_button_attention_style (self);
        main_label = nautilus_progress_info_get_status (info);
        nautilus_window_show_operation_notification (self->window,
                                                     main_label,
                                                     folder_to_open);
        g_free (main_label);
    }

    g_clear_object (&folder_to_open);
}

static void
disconnect_progress_infos (NautilusToolbar *self)
{
    GList *progress_infos;
    GList *l;

    progress_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    for (l = progress_infos; l != NULL; l = l->next)
    {
        g_signal_handlers_disconnect_by_data (l->data, self);
    }
}

static void
update_operations (NautilusToolbar *self)
{
    GList *progress_infos;
    GList *l;
    gboolean should_show_progress_button = FALSE;

    disconnect_progress_infos (self);
    g_list_store_remove_all (self->progress_infos_model);

    progress_infos = get_filtered_progress_infos (self);
    for (l = progress_infos; l != NULL; l = l->next)
    {
        should_show_progress_button = should_show_progress_button ||
                                      should_show_progress_info (l->data);

        g_signal_connect_swapped (l->data, "finished",
                                  G_CALLBACK (on_progress_info_finished), self);
        g_signal_connect_swapped (l->data, "cancelled",
                                  G_CALLBACK (on_progress_info_cancelled), self);
        g_signal_connect_swapped (l->data, "progress-changed",
                                  G_CALLBACK (on_progress_info_progress_changed), self);
        g_list_store_append (self->progress_infos_model, l->data);
    }

    g_list_free (progress_infos);

    if (should_show_progress_button &&
        !gtk_revealer_get_reveal_child (GTK_REVEALER (self->operations_revealer)))
    {
        add_operations_button_attention_style (self);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->operations_revealer),
                                       TRUE);
        gtk_widget_queue_draw (self->operations_icon);
    }

    /* Since we removed the info widgets, we need to restore the focus */
    if (gtk_widget_get_visible (self->operations_popover))
    {
        gtk_widget_grab_focus (self->operations_popover);
    }
}

static gboolean
on_progress_info_started_timeout (NautilusToolbar *self)
{
    GList *progress_infos;
    GList *filtered_progress_infos;

    update_operations (self);

    /* In case we didn't show the operations button because the operation total
     * time stimation is not good enough, update again to make sure we don't miss
     * a long time operation because of that */

    progress_infos = nautilus_progress_info_manager_get_all_infos (self->progress_manager);
    filtered_progress_infos = get_filtered_progress_infos (self);
    if (!nautilus_progress_manager_are_all_infos_finished_or_cancelled (self->progress_manager) &&
        g_list_length (progress_infos) != g_list_length (filtered_progress_infos))
    {
        g_list_free (filtered_progress_infos);
        return G_SOURCE_CONTINUE;
    }
    else
    {
        g_list_free (filtered_progress_infos);
        self->start_operations_timeout_id = 0;
        return G_SOURCE_REMOVE;
    }
}

static void
schedule_operations_start (NautilusToolbar *self)
{
    if (self->start_operations_timeout_id == 0)
    {
        /* Timeout is a little more than what we require for a stimated operation
         * total time, to make sure the stimated total time is correct */
        self->start_operations_timeout_id =
            g_timeout_add (SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE * 1000 + 500,
                           (GSourceFunc) on_progress_info_started_timeout,
                           self);
    }
}

static void
unschedule_operations_start (NautilusToolbar *self)
{
    if (self->start_operations_timeout_id != 0)
    {
        g_source_remove (self->start_operations_timeout_id);
        self->start_operations_timeout_id = 0;
    }
}

static void
on_progress_info_started (NautilusProgressInfo *info,
                          NautilusToolbar      *self)
{
    g_signal_handlers_disconnect_by_data (info, self);
    schedule_operations_start (self);
}

static void
on_new_progress_info (NautilusProgressInfoManager *manager,
                      NautilusProgressInfo        *info,
                      NautilusToolbar             *self)
{
    g_signal_connect (info, "started",
                      G_CALLBACK (on_progress_info_started), self);
}

static void
on_operations_icon_draw (GtkWidget       *widget,
                         cairo_t         *cr,
                         NautilusToolbar *self)
{
    gfloat elapsed_progress = 0;
    gint remaining_progress = 0;
    gint total_progress;
    gdouble ratio;
    GList *progress_infos;
    GList *l;
    guint width;
    guint height;
    gboolean all_cancelled;
    GdkRGBA background;
    GdkRGBA foreground;
    GtkStyleContext *style_context;

    style_context = gtk_widget_get_style_context (widget);
    gtk_style_context_get_color (style_context, gtk_style_context_get_state (style_context), &foreground);
    background = foreground;
    background.alpha *= 0.3;

    all_cancelled = TRUE;
    progress_infos = get_filtered_progress_infos (self);
    for (l = progress_infos; l != NULL; l = l->next)
    {
        if (!nautilus_progress_info_get_is_cancelled (l->data))
        {
            all_cancelled = FALSE;
            remaining_progress += nautilus_progress_info_get_remaining_time (l->data);
            elapsed_progress += nautilus_progress_info_get_elapsed_time (l->data);
        }
    }

    g_list_free (progress_infos);

    total_progress = remaining_progress + elapsed_progress;

    if (all_cancelled)
    {
        ratio = 1.0;
    }
    else
    {
        if (total_progress > 0)
        {
            ratio = MAX (0.05, elapsed_progress / total_progress);
        }
        else
        {
            ratio = 0.05;
        }
    }


    width = gtk_widget_get_allocated_width (widget);
    height = gtk_widget_get_allocated_height (widget);

    gdk_cairo_set_source_rgba (cr, &background);
    cairo_arc (cr,
               width / 2.0, height / 2.0,
               MIN (width, height) / 2.0,
               0, 2 * G_PI);
    cairo_fill (cr);
    cairo_move_to (cr, width / 2.0, height / 2.0);
    gdk_cairo_set_source_rgba (cr, &foreground);
    cairo_arc (cr,
               width / 2.0, height / 2.0,
               MIN (width, height) / 2.0,
               -G_PI / 2.0, ratio * 2 * G_PI - G_PI / 2.0);

    cairo_fill (cr);
}

static void
on_operations_popover_notify_visible (NautilusToolbar *self,
                                      GParamSpec      *pspec,
                                      GObject         *popover)
{
    if (gtk_widget_get_visible (GTK_WIDGET (popover)))
    {
        unschedule_remove_finished_operations (self);
        nautilus_progress_manager_add_viewer (self->progress_manager,
                                              G_OBJECT (self));
    }
    else
    {
        nautilus_progress_manager_remove_viewer (self->progress_manager,
                                                 G_OBJECT (self));
    }
}

static void
on_progress_has_viewers_changed (NautilusProgressInfoManager *manager,
                                 NautilusToolbar             *self)
{
    if (nautilus_progress_manager_has_viewers (manager))
    {
        unschedule_remove_finished_operations (self);
        return;
    }

    if (nautilus_progress_manager_are_all_infos_finished_or_cancelled (manager))
    {
        unschedule_remove_finished_operations (self);
        schedule_remove_finished_operations (self);
    }
}

static void
update_action (NautilusToolbar *self,
               const char      *action_name,
               gboolean         enabled)
{
    GAction *action;

    /* Activate/deactivate */
    action = g_action_map_lookup_action (G_ACTION_MAP (self->window), action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
undo_manager_changed (NautilusToolbar *self)
{
    NautilusFileUndoInfo *info;
    NautilusFileUndoManagerState undo_state;
    gboolean undo_active;
    gboolean redo_active;
    g_autofree gchar *undo_label = NULL;
    g_autofree gchar *redo_label = NULL;
    g_autofree gchar *undo_description = NULL;
    g_autofree gchar *redo_description = NULL;
    gboolean is_undo;
    g_autoptr (GMenu) updated_section = g_menu_new ();
    g_autoptr (GMenuItem) menu_item = NULL;

    /* Look up the last action from the undo manager, and get the text that
     * describes it, e.g. "Undo Create Folder"/"Redo Create Folder"
     */
    info = nautilus_file_undo_manager_get_action ();
    undo_state = nautilus_file_undo_manager_get_state ();
    undo_active = redo_active = FALSE;
    if (info != NULL && undo_state > NAUTILUS_FILE_UNDO_MANAGER_STATE_NONE)
    {
        is_undo = undo_state == NAUTILUS_FILE_UNDO_MANAGER_STATE_UNDO;

        /* The last action can either be undone/redone. Activate the corresponding
         * menu item and deactivate the other
         */
        undo_active = is_undo;
        redo_active = !is_undo;
        nautilus_file_undo_info_get_strings (info, &undo_label, &undo_description,
                                             &redo_label, &redo_description);
    }

    /* Set the label of the undo and redo menu items, and activate them appropriately
     */
    if (!undo_active || undo_label == NULL)
    {
        g_free (undo_label);
        undo_label = g_strdup (_("_Undo"));
    }
    g_set_object (&menu_item, g_menu_item_new (undo_label, "win.undo"));
    g_menu_append_item (updated_section, menu_item);
    update_action (self, "undo", undo_active);

    if (!redo_active || redo_label == NULL)
    {
        g_free (redo_label);
        redo_label = g_strdup (_("_Redo"));
    }
    g_set_object (&menu_item, g_menu_item_new (redo_label, "win.redo"));
    g_menu_append_item (updated_section, menu_item);
    update_action (self, "redo", redo_active);

    nautilus_gmenu_set_from_model (G_MENU (self->undo_redo_section),
                                   G_MENU_MODEL (updated_section));
}

static void
on_location_entry_close (GtkWidget       *close_button,
                         NautilusToolbar *self)
{
    nautilus_toolbar_set_show_location_entry (self, FALSE);
}

static void
on_location_entry_focus_changed (GObject    *object,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
    NautilusToolbar *toolbar;

    toolbar = NAUTILUS_TOOLBAR (user_data);

    if (gtk_widget_has_focus (GTK_WIDGET (object)))
    {
        toolbar->location_entry_should_auto_hide = TRUE;
    }
    else if (toolbar->location_entry_should_auto_hide)
    {
        nautilus_toolbar_set_show_location_entry (toolbar, FALSE);
    }
}

static GtkWidget *
operations_list_create_widget (GObject  *item,
                               gpointer  user_data)
{
    NautilusProgressInfo *info = NAUTILUS_PROGRESS_INFO (item);
    GtkWidget *widget;

    widget = nautilus_progress_info_widget_new (info);
    gtk_widget_show_all (widget);

    return widget;
}

static void
nautilus_toolbar_constructed (GObject *object)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    self->path_bar = GTK_WIDGET (g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL));
    gtk_box_append (GTK_BOX (self->path_bar_container),
                    self->path_bar);

    self->location_entry = nautilus_location_entry_new ();
    gtk_box_append (GTK_BOX (self->location_entry_container),
                    self->location_entry);
    self->location_entry_close_button = gtk_button_new_from_icon_name ("window-close-symbolic",
                                                                       GTK_ICON_SIZE_BUTTON);
    gtk_box_append (GTK_BOX (self->location_entry_container),
                    self->location_entry_close_button);
    g_signal_connect (self->location_entry_close_button, "clicked",
                      G_CALLBACK (on_location_entry_close), self);

    self->progress_manager = nautilus_progress_info_manager_dup_singleton ();
    g_signal_connect (self->progress_manager, "new-progress-info",
                      G_CALLBACK (on_new_progress_info), self);
    g_signal_connect (self->progress_manager, "has-viewers-changed",
                      G_CALLBACK (on_progress_has_viewers_changed), self);

    self->progress_infos_model = g_list_store_new (NAUTILUS_TYPE_PROGRESS_INFO);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->operations_list),
                             G_LIST_MODEL (self->progress_infos_model),
                             (GtkListBoxCreateWidgetFunc) operations_list_create_widget,
                             NULL,
                             NULL);
    update_operations (self);

    self->back_button_longpress_gesture = gtk_gesture_long_press_new (self->back_button);
    g_signal_connect (self->back_button_longpress_gesture, "pressed",
                      G_CALLBACK (back_button_longpress_cb), self);

    self->forward_button_longpress_gesture = gtk_gesture_long_press_new (self->forward_button);
    g_signal_connect (self->forward_button_longpress_gesture, "pressed",
                      G_CALLBACK (forward_button_longpress_cb), self);

    g_object_set_data (G_OBJECT (self->back_button), "nav-direction",
                       GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_BACK));
    g_object_set_data (G_OBJECT (self->forward_button), "nav-direction",
                       GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_FORWARD));


    self->back_button_multi_press_gesture = gtk_gesture_multi_press_new (self->back_button);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->back_button_multi_press_gesture),
                                   GDK_BUTTON_SECONDARY);
    g_signal_connect (self->back_button_multi_press_gesture, "pressed",
                      G_CALLBACK (navigation_button_press_cb), self);

    self->forward_button_multi_press_gesture = gtk_gesture_multi_press_new (self->forward_button);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->forward_button_multi_press_gesture),
                                   GDK_BUTTON_SECONDARY);
    g_signal_connect (self->forward_button_multi_press_gesture, "pressed",
                      G_CALLBACK (navigation_button_press_cb), self);

    g_signal_connect (self->operations_popover, "show",
                      (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (self->operations_popover, "closed",
                              (GCallback) gtk_widget_grab_focus, self);
    g_signal_connect (self->location_entry, "notify::has-focus",
                      G_CALLBACK (on_location_entry_focus_changed), self);

    /* Setting a max width on one entry to effectively set a max expansion for
     * the whole title widget. */
    gtk_entry_set_max_width_chars (GTK_ENTRY (self->location_entry), 88);

    gtk_widget_show_all (GTK_WIDGET (self));
    toolbar_update_appearance (self);
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

void
nautilus_toolbar_on_window_constructed (NautilusToolbar *self)
{
    /* undo_manager_changed manipulates the window actions, so set it up
     * after the window and it's actions have been constructed
     */
    g_signal_connect_object (nautilus_file_undo_manager_get (),
                             "undo-changed",
                             G_CALLBACK (undo_manager_changed),
                             self,
                             G_CONNECT_SWAPPED);

    undo_manager_changed (self);
}

static void
nautilus_toolbar_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    switch (property_id)
    {
        case PROP_SHOW_LOCATION_ENTRY:
        {
            g_value_set_boolean (value, self->show_location_entry);
        }
        break;

        case PROP_SEARCHING:
        {
            g_value_set_boolean (value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button)));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
on_window_slot_destroyed (gpointer  data,
                          GObject  *where_the_object_was)
{
    NautilusToolbar *self;

    self = NAUTILUS_TOOLBAR (data);

    /* The window slot was finalized, and the binding has already been removed.
     * Null it here, so that dispose() does not trip over itself when removing it.
     */
    self->icon_binding = NULL;
    self->search_binding = NULL;

    nautilus_toolbar_set_window_slot_real (self, NULL);
}

static void
on_window_focus_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
    GtkWidget *widget;
    NautilusToolbar *toolbar;

    widget = GTK_WIDGET (object);
    toolbar = NAUTILUS_TOOLBAR (user_data);

    if (g_settings_get_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
    {
        return;
    }

    /* The working assumption being made here is, if the location entry is visible,
     * the user must have switched windows while having keyboard focus on the entry
     * (because otherwise it would be invisible),
     * so we focus the entry explicitly to reset the “should auto-hide” flag.
     */
    if (gtk_widget_has_focus (widget) && toolbar->show_location_entry)
    {
        gtk_widget_grab_focus (toolbar->location_entry);
    }
    /* The location entry in general is hidden when it loses focus,
     * but hiding it when switching windows could be undesirable, as the user
     * might want to copy a path from somewhere. This here prevents that from happening.
     */
    else
    {
        toolbar->location_entry_should_auto_hide = FALSE;
    }
}

static void
nautilus_toolbar_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

    switch (property_id)
    {
        case PROP_WINDOW:
        {
            if (self->window != NULL)
            {
                g_signal_handlers_disconnect_by_func (self->window,
                                                      on_window_focus_changed, self);
            }
            self->window = g_value_get_object (value);
            if (self->window != NULL)
            {
                g_signal_connect (self->window, "notify::has-focus",
                                  G_CALLBACK (on_window_focus_changed), self);
            }
        }
        break;

        case PROP_SHOW_LOCATION_ENTRY:
        {
            nautilus_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
        }
        break;

        case PROP_WINDOW_SLOT:
        {
            nautilus_toolbar_set_window_slot (self, g_value_get_object (value));
        }
        break;

        case PROP_SEARCHING:
        {
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button),
                                          g_value_get_boolean (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_toolbar_dispose (GObject *object)
{
    NautilusToolbar *self;

    self = NAUTILUS_TOOLBAR (object);

    g_clear_object (&self->forward_button_multi_press_gesture);
    g_clear_object (&self->back_button_multi_press_gesture);
    g_clear_pointer (&self->icon_binding, g_binding_unbind);
    g_clear_pointer (&self->search_binding, g_binding_unbind);

    G_OBJECT_CLASS (nautilus_toolbar_parent_class)->dispose (object);
}

static void
nautilus_toolbar_finalize (GObject *obj)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          toolbar_update_appearance, self);

    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        g_object_weak_unref (G_OBJECT (self->window_slot),
                             on_window_slot_destroyed, self);
        self->window_slot = NULL;
    }
    disconnect_progress_infos (self);
    unschedule_remove_finished_operations (self);
    unschedule_operations_start (self);
    unschedule_operations_button_attention_style (self);

    g_clear_object (&self->progress_infos_model);
    g_signal_handlers_disconnect_by_data (self->progress_manager, self);
    g_clear_object (&self->progress_manager);

    g_signal_handlers_disconnect_by_func (self->window,
                                          on_window_focus_changed, self);

    g_clear_object (&self->back_button_longpress_gesture);
    g_clear_object (&self->forward_button_longpress_gesture);

    G_OBJECT_CLASS (nautilus_toolbar_parent_class)->finalize (obj);
}

static void
nautilus_toolbar_class_init (NautilusToolbarClass *klass)
{
    GObjectClass *oclass;
    GtkWidgetClass *widget_class;

    widget_class = GTK_WIDGET_CLASS (klass);
    oclass = G_OBJECT_CLASS (klass);
    oclass->get_property = nautilus_toolbar_get_property;
    oclass->set_property = nautilus_toolbar_set_property;
    oclass->dispose = nautilus_toolbar_dispose;
    oclass->finalize = nautilus_toolbar_finalize;
    oclass->constructed = nautilus_toolbar_constructed;

    properties[PROP_WINDOW] =
        g_param_spec_object ("window",
                             "The NautilusWindow",
                             "The NautilusWindow this toolbar is part of",
                             NAUTILUS_TYPE_WINDOW,
                             G_PARAM_WRITABLE |
                             G_PARAM_STATIC_STRINGS);
    properties[PROP_SHOW_LOCATION_ENTRY] =
        g_param_spec_boolean ("show-location-entry",
                              "Whether to show the location entry",
                              "Whether to show the location entry instead of the pathbar",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties [PROP_WINDOW_SLOT] =
        g_param_spec_object ("window-slot",
                             "Window slot currently active",
                             "Window slot currently acive",
                             NAUTILUS_TYPE_WINDOW_SLOT,
                             (G_PARAM_READWRITE |
                              G_PARAM_STATIC_STRINGS));

    properties [PROP_SEARCHING] =
        g_param_spec_boolean ("searching",
                              "Current view is searching",
                              "Whether the current view is searching or not",
                              FALSE,
                              G_PARAM_READWRITE);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-toolbar.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_popover);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_list);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_toggle_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_toggle_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, app_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, undo_redo_section);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, back_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, back_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, forward_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, forward_menu);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, toolbar_switcher);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, path_bar_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, location_entry_container);

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, search_button);

    gtk_widget_class_bind_template_callback (widget_class, on_operations_icon_draw);
    gtk_widget_class_bind_template_callback (widget_class, on_operations_popover_notify_visible);
}

GtkWidget *
nautilus_toolbar_new ()
{
    return g_object_new (NAUTILUS_TYPE_TOOLBAR,
                         NULL);
}

GtkWidget *
nautilus_toolbar_get_path_bar (NautilusToolbar *self)
{
    return self->path_bar;
}

GtkWidget *
nautilus_toolbar_get_location_entry (NautilusToolbar *self)
{
    return self->location_entry;
}

void
nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
                                          gboolean         show_location_entry)
{
    if (show_location_entry != self->show_location_entry)
    {
        self->show_location_entry = show_location_entry;
        toolbar_update_appearance (self);

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LOCATION_ENTRY]);
    }
}

static void
box_remove_all_children (GtkBox *box)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child (GTK_WIDGET (box))) != NULL)
    {
        gtk_box_remove (GTK_BOX (box), child);
    }
}

static void
slot_on_extensions_background_menu_changed (NautilusToolbar    *self,
                                            GParamSpec         *param,
                                            NautilusWindowSlot *slot)
{
    g_autoptr (GMenuModel) menu = NULL;

    menu = nautilus_window_slot_get_extensions_background_menu (slot);
    nautilus_path_bar_set_extensions_background_menu (NAUTILUS_PATH_BAR (self->path_bar),
                                                      menu);
}

static void
slot_on_templates_menu_changed (NautilusToolbar    *self,
                                GParamSpec         *param,
                                NautilusWindowSlot *slot)
{
    g_autoptr (GMenuModel) menu = NULL;

    menu = nautilus_window_slot_get_templates_menu (slot);
    nautilus_path_bar_set_templates_menu (NAUTILUS_PATH_BAR (self->path_bar),
                                          menu);
}

static void
on_slot_toolbar_menu_sections_changed (NautilusToolbar    *self,
                                       GParamSpec         *param,
                                       NautilusWindowSlot *slot)
{
    NautilusToolbarMenuSections *new_sections;
    g_autoptr (GMenuItem) zoom_item = NULL;
    g_autoptr (GMenuItem) sort_item = NULL;

    new_sections = nautilus_window_slot_get_toolbar_menu_sections (slot);

    gtk_widget_set_sensitive (self->view_button, (new_sections != NULL));
    if (new_sections == NULL)
    {
        return;
    }

    /* Let's assume that zoom and sort sections are the first and second items
     * in view_menu, as per nautilus-toolbar.ui. */

    zoom_item = g_menu_item_new_from_model (self->view_menu, 0);
    g_menu_remove (G_MENU (self->view_menu), 0);
    g_menu_item_set_section (zoom_item, new_sections->zoom_section);
    g_menu_insert_item (G_MENU (self->view_menu), 0, zoom_item);

    sort_item = g_menu_item_new_from_model (self->view_menu, 1);
    g_menu_remove (G_MENU (self->view_menu), 1);
    g_menu_item_set_section (sort_item, new_sections->sort_section);
    g_menu_insert_item (G_MENU (self->view_menu), 1, sort_item);
}


static void
disconnect_toolbar_menu_sections_change_handler (NautilusToolbar *self)
{
    if (self->window_slot == NULL)
    {
        return;
    }

    g_signal_handlers_disconnect_by_func (self->window_slot,
                                          G_CALLBACK (on_slot_toolbar_menu_sections_changed),
                                          self);
}

static gboolean
nautilus_toolbar_view_toggle_icon_transform_to (GBinding     *binding,
                                                const GValue *from_value,
                                                GValue       *to_value,
                                                gpointer      user_data)
{
    GIcon *icon;

    icon = g_value_get_object (from_value);

    /* As per design decision, we let the previous used icon if no
     * view menu is available */
    if (icon)
    {
        g_value_set_object (to_value, icon);
    }

    return TRUE;
}

static gboolean
nautilus_toolbar_view_toggle_tooltip_transform_to (GBinding     *binding,
                                                   const GValue *from_value,
                                                   GValue       *to_value,
                                                   gpointer      user_data)
{
    const gchar *tooltip;

    tooltip = g_value_get_string (from_value);

    /* As per design decision, we let the previous used tooltip if no
     * view menu is available */
    if (tooltip)
    {
        g_value_set_string (to_value, tooltip);
    }

    return TRUE;
}

/* Called from on_window_slot_destroyed(), since bindings and signal handlers
 * are automatically removed once the slot goes away.
 */
static void
nautilus_toolbar_set_window_slot_real (NautilusToolbar    *self,
                                       NautilusWindowSlot *slot)
{
    g_autoptr (GList) children = NULL;

    self->window_slot = slot;

    if (self->window_slot != NULL)
    {
        g_object_weak_ref (G_OBJECT (self->window_slot),
                           on_window_slot_destroyed,
                           self);

        self->icon_binding = g_object_bind_property_full (self->window_slot, "icon",
                                                          self->view_toggle_icon, "gicon",
                                                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                                          (GBindingTransformFunc) nautilus_toolbar_view_toggle_icon_transform_to,
                                                          NULL,
                                                          self,
                                                          NULL);

        self->tooltip_binding = g_object_bind_property_full (self->window_slot, "tooltip",
                                                             self->view_toggle_button, "tooltip-text",
                                                             G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                                             (GBindingTransformFunc) nautilus_toolbar_view_toggle_tooltip_transform_to,
                                                             NULL,
                                                             self,
                                                             NULL);

        self->search_binding = g_object_bind_property (self->window_slot, "searching",
                                                       self, "searching",
                                                       G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

        on_slot_toolbar_menu_sections_changed (self, NULL, self->window_slot);
        g_signal_connect_swapped (self->window_slot, "notify::toolbar-menu-sections",
                                  G_CALLBACK (on_slot_toolbar_menu_sections_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::extensions-background-menu",
                                  G_CALLBACK (slot_on_extensions_background_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::templates-menu",
                                  G_CALLBACK (slot_on_templates_menu_changed), self);
        g_signal_connect_swapped (self->window_slot, "notify::searching",
                                  G_CALLBACK (toolbar_update_appearance), self);
    }

    box_remove_all_children (GTK_BOX (self->search_container));

    if (self->window_slot != NULL)
    {
        gtk_box_append (GTK_BOX (self->search_container),
                        GTK_WIDGET (nautilus_window_slot_get_query_editor (self->window_slot)));
    }

    toolbar_update_appearance (self);

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WINDOW_SLOT]);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCHING]);
}

void
nautilus_toolbar_set_window_slot (NautilusToolbar    *self,
                                  NautilusWindowSlot *window_slot)
{
    g_return_if_fail (NAUTILUS_IS_TOOLBAR (self));
    g_return_if_fail (window_slot == NULL || NAUTILUS_IS_WINDOW_SLOT (window_slot));

    if (self->window_slot == window_slot)
    {
        return;
    }

    g_clear_pointer (&self->icon_binding, g_binding_unbind);
    g_clear_pointer (&self->search_binding, g_binding_unbind);

    disconnect_toolbar_menu_sections_change_handler (self);
    if (self->window_slot != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->window_slot, self);
        g_object_weak_unref (G_OBJECT (self->window_slot),
                             on_window_slot_destroyed, self);
    }

    nautilus_toolbar_set_window_slot_real (self, window_slot);
}

gboolean
nautilus_toolbar_is_menu_visible (NautilusToolbar *self)
{
    GtkWidget *menu;

    g_return_val_if_fail (NAUTILUS_IS_TOOLBAR (self), FALSE);

    menu = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (self->app_button)));
    g_return_val_if_fail (menu != NULL, FALSE);

    return gtk_widget_is_visible (menu);
}

gboolean
nautilus_toolbar_is_operations_button_active (NautilusToolbar *self)
{
    return gtk_widget_is_visible (GTK_WIDGET (self->operations_popover));
}
