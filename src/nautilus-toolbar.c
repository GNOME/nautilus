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

#include <config.h>

#include "nautilus-toolbar.h"

#include "nautilus-location-entry.h"
#include "nautilus-pathbar.h"
#include "nautilus-window.h"
#include "nautilus-progress-info-widget.h"
#include "nautilus-application.h"

#include "nautilus-global-preferences.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-progress-info-manager.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-toolbar-menu-sections.h"

#include "animation/ide-box-theatric.h"
#include "animation/egg-animation.h"

#include <glib/gi18n.h>
#include <math.h>

#define OPERATION_MINIMUM_TIME 2 /*s */
#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 3 /*s */

#define ANIMATION_X_GROW 30
#define ANIMATION_Y_GROW 30

typedef enum
{
    NAUTILUS_NAVIGATION_DIRECTION_NONE,
    NAUTILUS_NAVIGATION_DIRECTION_BACK,
    NAUTILUS_NAVIGATION_DIRECTION_FORWARD
} NautilusNavigationDirection;

struct _NautilusToolbar
{
    GtkHeaderBar parent_instance;

    NautilusWindow *window;

    GtkWidget *path_bar_container;
    GtkWidget *location_entry_container;
    GtkWidget *path_bar;
    GtkWidget *location_entry;

    gboolean show_location_entry;
    gboolean location_entry_should_auto_hide;

    guint popup_timeout_id;
    guint start_operations_timeout_id;
    guint remove_finished_operations_timeout_id;
    guint operations_button_attention_timeout_id;

    GtkWidget *operations_button;
    GtkWidget *view_button;
    GtkWidget *view_menu_zoom_section;
    GtkWidget *view_menu_undo_redo_section;
    GtkWidget *view_menu_extended_section;
    GtkWidget *undo_button;
    GtkWidget *redo_button;
    GtkWidget *view_toggle_button;
    GtkWidget *view_toggle_icon;

    GtkWidget *operations_popover;
    GtkWidget *operations_container;
    GtkWidget *operations_revealer;
    GtkWidget *operations_icon;

    GtkWidget *forward_button;
    GtkWidget *back_button;

    NautilusProgressInfoManager *progress_manager;

    /* active slot & bindings */
    NautilusWindowSlot *active_slot;
    GBinding *icon_binding;
    GBinding *view_widget_binding;
};

enum
{
    PROP_WINDOW = 1,
    PROP_SHOW_LOCATION_ENTRY,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusToolbar, nautilus_toolbar, GTK_TYPE_HEADER_BAR);

static void unschedule_menu_popup_timeout (NautilusToolbar *self);
static void update_operations (NautilusToolbar *self);

static void
toolbar_update_appearance (NautilusToolbar *self)
{
    gboolean show_location_entry;

    show_location_entry = self->show_location_entry ||
                          g_settings_get_boolean (nautilus_preferences,
                                                  NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

    gtk_widget_set_visible (self->location_entry,
                            show_location_entry);
    gtk_widget_set_visible (self->path_bar,
                            !show_location_entry);
}

static void
activate_back_or_forward_menu_item (GtkMenuItem    *menu_item,
                                    NautilusWindow *window,
                                    gboolean        back)
{
    int index;

    g_assert (GTK_IS_MENU_ITEM (menu_item));

    index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

    nautilus_window_back_or_forward (window, back, index, nautilus_event_get_window_open_flags ());
}

static void
activate_back_menu_item_callback (GtkMenuItem    *menu_item,
                                  NautilusWindow *window)
{
    activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem    *menu_item,
                                     NautilusWindow *window)
{
    activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static void
fill_menu (NautilusWindow *window,
           GtkWidget      *menu,
           gboolean        back)
{
    NautilusWindowSlot *slot;
    GtkWidget *menu_item;
    int index;
    GList *list;

    slot = nautilus_window_get_active_slot (window);
    list = back ? nautilus_window_slot_get_back_history (slot) :
           nautilus_window_slot_get_forward_history (slot);

    index = 0;
    while (list != NULL)
    {
        menu_item = nautilus_bookmark_menu_item_new (NAUTILUS_BOOKMARK (list->data));
        g_object_set_data (G_OBJECT (menu_item), "user_data", GINT_TO_POINTER (index));
        gtk_widget_show (GTK_WIDGET (menu_item));
        g_signal_connect_object (menu_item, "activate",
                                 back
                                 ? G_CALLBACK (activate_back_menu_item_callback)
                                 : G_CALLBACK (activate_forward_menu_item_callback),
                                 window, 0);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
        list = g_list_next (list);
        ++index;
    }
}

static void
show_menu (NautilusToolbar *self,
           GtkWidget       *widget,
           const GdkEvent  *event)
{
    NautilusWindow *window;
    GtkWidget *menu;
    NautilusNavigationDirection direction;

    window = self->window;
    menu = gtk_menu_new ();

    direction = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
                                                     "nav-direction"));

    switch (direction)
    {
        case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
        {
            fill_menu (window, menu, FALSE);
        }
        break;

        case NAUTILUS_NAVIGATION_DIRECTION_BACK:
        {
            fill_menu (window, menu, TRUE);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (window), NULL);
    gtk_menu_popup_at_widget (GTK_MENU (menu), widget,
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              event);
}

#define MENU_POPUP_TIMEOUT 1200

typedef struct
{
    NautilusToolbar *self;
    GtkWidget *widget;
    GdkEvent *event;
} ScheduleMenuData;

static void
schedule_menu_data_free (ScheduleMenuData *data)
{
    gdk_event_free (data->event);
    g_slice_free (ScheduleMenuData, data);
}

static gboolean
popup_menu_timeout_cb (gpointer user_data)
{
    ScheduleMenuData *data = user_data;

    show_menu (data->self, data->widget, data->event);

    /* Need to also reset the ID here. */
    unschedule_menu_popup_timeout (data->self);

    return G_SOURCE_REMOVE;
}

static void
unschedule_menu_popup_timeout (NautilusToolbar *self)
{
    if (self->popup_timeout_id != 0)
    {
        g_source_remove (self->popup_timeout_id);
        self->popup_timeout_id = 0;
    }
}

static void
schedule_menu_popup_timeout (NautilusToolbar *self,
                             GtkWidget       *widget,
                             GdkEvent        *event)
{
    ScheduleMenuData *data;

    /* unschedule any previous timeouts */
    unschedule_menu_popup_timeout (self);

    data = g_slice_new0 (ScheduleMenuData);
    data->self = self;
    data->widget = widget;
    data->event = gdk_event_copy (event);

    self->popup_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, MENU_POPUP_TIMEOUT,
                                                 popup_menu_timeout_cb, data,
                                                 (GDestroyNotify) schedule_menu_data_free);
}
static gboolean
navigation_button_press_cb (GtkButton *button,
                            GdkEvent  *event,
                            gpointer   user_data)
{
    NautilusToolbar *self = user_data;
    GdkEventButton *button_event;

    button_event = (GdkEventButton *) event;

    if (button_event->button == 3)
    {
        /* right click */
        show_menu (self, GTK_WIDGET (button), event);
        return TRUE;
    }

    if (button_event->button == 1)
    {
        schedule_menu_popup_timeout (self, GTK_WIDGET (button), event);
    }

    return FALSE;
}

static gboolean
navigation_button_release_cb (GtkButton      *button,
                              GdkEventButton *event,
                              gpointer        user_data)
{
    NautilusToolbar *self = user_data;

    unschedule_menu_popup_timeout (self);

    return FALSE;
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
    GtkWidget *progress;
    gboolean should_show_progress_button = FALSE;

    gtk_container_foreach (GTK_CONTAINER (self->operations_container),
                           (GtkCallback) gtk_widget_destroy,
                           NULL);

    disconnect_progress_infos (self);

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
        progress = nautilus_progress_info_widget_new (l->data);
        gtk_box_pack_start (GTK_BOX (self->operations_container),
                            progress,
                            FALSE, FALSE, 0);
    }

    g_list_free (progress_infos);

    if (should_show_progress_button &&
        !gtk_revealer_get_reveal_child (GTK_REVEALER (self->operations_revealer)))
    {
        add_operations_button_attention_style (self);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->operations_revealer),
                                       TRUE);
        gtk_widget_queue_draw (self->operations_icon);

        /* Show the popover at start to increase visibility.
         * Check whether the toolbar is visible or not before showing the
         * popover. This can happens if the window has the disables-chrome
         * property set. */
        if (gtk_widget_is_visible (GTK_WIDGET (self)))
        {
            GtkAllocation rect;
            IdeBoxTheatric *theatric;

            gtk_widget_get_allocation (GTK_WIDGET (self->operations_button), &rect);
            theatric = g_object_new (IDE_TYPE_BOX_THEATRIC,
                                     "alpha", 0.9,
                                     "background", "#fdfdfd",
                                     "target", self->operations_button,
                                     "height", rect.height,
                                     "width", rect.width,
                                     "x", rect.x,
                                     "y", rect.y,
                                     NULL);

            egg_object_animate_full (theatric,
                                     EGG_ANIMATION_EASE_IN_CUBIC,
                                     250,
                                     gtk_widget_get_frame_clock (GTK_WIDGET (self->operations_button)),
                                     g_object_unref,
                                     theatric,
                                     "x", rect.x - ANIMATION_X_GROW,
                                     "width", rect.width + (ANIMATION_X_GROW * 2),
                                     "y", rect.y - ANIMATION_Y_GROW,
                                     "height", rect.height + (ANIMATION_Y_GROW * 2),
                                     "alpha", 0.0,
                                     NULL);
        }
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
    gtk_style_context_get_color (style_context, gtk_widget_get_state_flags (widget), &foreground);
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
on_operations_button_toggled (NautilusToolbar *self,
                              GtkToggleButton *button)
{
    if (gtk_toggle_button_get_active (button))
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
update_menu_item (GtkWidget       *menu_item,
                  NautilusToolbar *self,
                  const char      *action_name,
                  gboolean         enabled,
                  char            *label)
{
    GAction *action;
    GValue val = G_VALUE_INIT;

    /* Activate/deactivate */
    action = g_action_map_lookup_action (G_ACTION_MAP (self->window), action_name);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

    /* Set the text of the menu item. Can't use gtk_button_set_label here as
     * we need to set the text property, not the label. There's no equivalent
     * gtk_model_button_set_text function (refer to #766083 for discussion
     * on adding a set_text function)
     */
    g_value_init (&val, G_TYPE_STRING);
    g_value_set_string (&val, label);
    g_object_set_property (G_OBJECT (menu_item), "text", &val);
    g_value_unset (&val);
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
    update_menu_item (self->undo_button, self, "undo", undo_active, undo_label);

    if (!redo_active || redo_label == NULL)
    {
        g_free (redo_label);
        redo_label = g_strdup (_("_Redo"));
    }
    update_menu_item (self->redo_button, self, "redo", redo_active, redo_label);
}

static gboolean
on_location_entry_populate_popup (GtkEntry  *entry,
                                  GtkWidget *widget,
                                  gpointer   user_data)
{
    NautilusToolbar *toolbar;

    toolbar = user_data;

    toolbar->location_entry_should_auto_hide = FALSE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_location_entry_focus_out_event (GtkWidget *widget,
                                   GdkEvent  *event,
                                   gpointer   user_data)
{
    NautilusToolbar *toolbar;

    toolbar = user_data;

    if (toolbar->location_entry_should_auto_hide)
    {
        nautilus_toolbar_set_show_location_entry (toolbar, FALSE);
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_location_entry_focus_in_event (GtkWidget *widget,
                                  GdkEvent  *event,
                                  gpointer   user_data)
{
    NautilusToolbar *toolbar;

    toolbar = user_data;

    toolbar->location_entry_should_auto_hide = TRUE;

    return GDK_EVENT_PROPAGATE;
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
    GtkBuilder *builder;
    GtkWidget *menu_popover;

    gtk_widget_init_template (GTK_WIDGET (self));

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-toolbar-menu.ui");
    menu_popover = GTK_WIDGET (gtk_builder_get_object (builder, "menu_popover"));

    self->window = NULL;
    self->view_menu_zoom_section = GTK_WIDGET (gtk_builder_get_object (builder, "view_menu_zoom_section"));
    self->view_menu_undo_redo_section = GTK_WIDGET (gtk_builder_get_object (builder, "view_menu_undo_redo_section"));
    self->view_menu_extended_section = GTK_WIDGET (gtk_builder_get_object (builder, "view_menu_extended_section"));
    self->undo_button = GTK_WIDGET (gtk_builder_get_object (builder, "undo"));
    self->redo_button = GTK_WIDGET (gtk_builder_get_object (builder, "redo"));
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->view_button), menu_popover);
    g_object_unref (builder);

    self->path_bar = g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL);
    gtk_container_add (GTK_CONTAINER (self->path_bar_container),
                       self->path_bar);

    self->location_entry = nautilus_location_entry_new ();
    gtk_container_add (GTK_CONTAINER (self->location_entry_container),
                       self->location_entry);

    self->progress_manager = nautilus_progress_info_manager_dup_singleton ();
    g_signal_connect (self->progress_manager, "new-progress-info",
                      G_CALLBACK (on_new_progress_info), self);
    g_signal_connect (self->progress_manager, "has-viewers-changed",
                      G_CALLBACK (on_progress_has_viewers_changed), self);

    update_operations (self);

    g_object_set_data (G_OBJECT (self->back_button), "nav-direction",
                       GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_BACK));
    g_object_set_data (G_OBJECT (self->forward_button), "nav-direction",
                       GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_FORWARD));
    g_signal_connect (self->back_button, "button-press-event",
                      G_CALLBACK (navigation_button_press_cb), self);
    g_signal_connect (self->back_button, "button-release-event",
                      G_CALLBACK (navigation_button_release_cb), self);
    g_signal_connect (self->forward_button, "button-press-event",
                      G_CALLBACK (navigation_button_press_cb), self);
    g_signal_connect (self->forward_button, "button-release-event",
                      G_CALLBACK (navigation_button_release_cb), self);
    g_signal_connect (self->operations_popover, "show",
                      (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (self->operations_popover, "closed",
                              (GCallback) gtk_widget_grab_focus, self);
    g_signal_connect (self->location_entry, "populate-popup",
                      G_CALLBACK (on_location_entry_populate_popup), self);
    g_signal_connect (self->location_entry, "focus-out-event",
                      G_CALLBACK (on_location_entry_focus_out_event), self);
    g_signal_connect (self->location_entry, "focus-in-event",
                      G_CALLBACK (on_location_entry_focus_in_event), self);

    gtk_widget_show_all (GTK_WIDGET (self));
    toolbar_update_appearance (self);
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

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

/* The working assumption being made here is, if the location entry is visible,
 * the user must have switched windows while having keyboard focus on the entry
 * (because otherwise it would be invisible),
 * so we focus the entry explicitly to reset the “should auto-hide” flag.
 */
static gboolean
on_window_focus_in_event (GtkWidget *widget,
                          GdkEvent  *event,
                          gpointer   user_data)
{
    NautilusToolbar *toolbar;

    toolbar = user_data;

    if (g_settings_get_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
    {
        return GDK_EVENT_PROPAGATE;
    }

    if (toolbar->show_location_entry)
    {
        gtk_widget_grab_focus (toolbar->location_entry);
    }

    return GDK_EVENT_PROPAGATE;
}

/* The location entry in general is hidden when it loses focus,
 * but hiding it when switching windows could be undesirable, as the user
 * might want to copy a path from somewhere. This here prevents that from happening.
 */
static gboolean
on_window_focus_out_event (GtkWidget *widget,
                           GdkEvent  *event,
                           gpointer   user_data)
{
    NautilusToolbar *toolbar;

    toolbar = user_data;

    if (g_settings_get_boolean (nautilus_preferences,
                                NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY))
    {
        return GDK_EVENT_PROPAGATE;
    }

    toolbar->location_entry_should_auto_hide = FALSE;

    return GDK_EVENT_PROPAGATE;
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
                                                      on_window_focus_in_event, self);
                g_signal_handlers_disconnect_by_func (self->window,
                                                      on_window_focus_out_event, self);
            }
            self->window = g_value_get_object (value);
            if (self->window != NULL)
            {
                g_signal_connect (self->window, "focus-in-event",
                                  G_CALLBACK (on_window_focus_in_event), self);
                g_signal_connect (self->window, "focus-out-event",
                                  G_CALLBACK (on_window_focus_out_event), self);
            }
        }
        break;

        case PROP_SHOW_LOCATION_ENTRY:
        {
            nautilus_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
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
nautilus_toolbar_finalize (GObject *obj)
{
    NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);

    g_signal_handlers_disconnect_by_func (nautilus_preferences,
                                          toolbar_update_appearance, self);
    disconnect_progress_infos (self);
    unschedule_menu_popup_timeout (self);
    unschedule_remove_finished_operations (self);
    unschedule_operations_start (self);
    unschedule_operations_button_attention_style (self);

    g_signal_handlers_disconnect_by_data (self->progress_manager, self);
    g_clear_object (&self->progress_manager);

    g_signal_handlers_disconnect_by_func (self->window,
                                          on_window_focus_in_event, self);
    g_signal_handlers_disconnect_by_func (self->window,
                                          on_window_focus_out_event, self);

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
    oclass->finalize = nautilus_toolbar_finalize;

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

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-toolbar.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_popover);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, operations_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_toggle_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, view_toggle_icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, path_bar_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, location_entry_container);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, back_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusToolbar, forward_button);

    gtk_widget_class_bind_template_callback (widget_class, on_operations_icon_draw);
    gtk_widget_class_bind_template_callback (widget_class, on_operations_button_toggled);
}

GtkWidget *
nautilus_toolbar_new ()
{
    return g_object_new (NAUTILUS_TYPE_TOOLBAR,
                         "show-close-button", TRUE,
                         "custom-title", gtk_label_new (NULL),
                         "valign", GTK_ALIGN_CENTER,
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

static void
container_remove_all_children (GtkContainer *container)
{
    GList *children;
    GList *child;

    children = gtk_container_get_children (container);
    for (child = children; child != NULL; child = g_list_next (child))
    {
        gtk_container_remove (container, GTK_WIDGET (child->data));
    }
    g_list_free (children);
}

static void
on_slot_toolbar_menu_sections_changed (NautilusToolbar    *toolbar,
                                       GParamSpec         *param,
                                       NautilusWindowSlot *slot)
{
    NautilusToolbarMenuSections *new_sections;

    container_remove_all_children (GTK_CONTAINER (toolbar->view_menu_zoom_section));
    container_remove_all_children (GTK_CONTAINER (toolbar->view_menu_extended_section));

    new_sections = nautilus_window_slot_get_toolbar_menu_sections (slot);
    if (new_sections == NULL)
    {
        return;
    }

    gtk_widget_set_visible (toolbar->view_menu_undo_redo_section,
                            new_sections->supports_undo_redo);

    if (new_sections->zoom_section != NULL)
    {
        gtk_box_pack_start (GTK_BOX (toolbar->view_menu_zoom_section), new_sections->zoom_section, FALSE, FALSE, 0);
    }

    if (new_sections->extended_section != NULL)
    {
        gtk_box_pack_start (GTK_BOX (toolbar->view_menu_extended_section), new_sections->extended_section, FALSE, FALSE, 0);
    }
}

static void
disconnect_toolbar_menu_sections_change_handler (NautilusToolbar *toolbar)
{
    if (toolbar->active_slot == NULL)
    {
        return;
    }

    g_signal_handlers_disconnect_by_func (toolbar->active_slot,
                                          G_CALLBACK (on_slot_toolbar_menu_sections_changed),
                                          toolbar);
}

void
nautilus_toolbar_set_active_slot (NautilusToolbar    *toolbar,
                                  NautilusWindowSlot *slot)
{
    g_return_if_fail (NAUTILUS_IS_TOOLBAR (toolbar));

    g_clear_pointer (&toolbar->icon_binding, g_binding_unbind);
    g_clear_pointer (&toolbar->view_widget_binding, g_binding_unbind);

    if (toolbar->active_slot != slot)
    {
        disconnect_toolbar_menu_sections_change_handler (toolbar);
        toolbar->active_slot = slot;

        if (slot)
        {
            toolbar->icon_binding =
                g_object_bind_property_full (slot, "icon",
                                             toolbar->view_toggle_icon, "gicon",
                                             G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                             (GBindingTransformFunc) nautilus_toolbar_view_toggle_icon_transform_to,
                                             NULL,
                                             toolbar,
                                             NULL);

            on_slot_toolbar_menu_sections_changed (toolbar, NULL, slot);
            g_signal_connect_swapped (slot, "notify::toolbar-menu-sections",
                                      G_CALLBACK (on_slot_toolbar_menu_sections_changed), toolbar);
        }
    }
}

gboolean
nautilus_toolbar_is_menu_visible (NautilusToolbar *self)
{
    GtkPopover *popover;

    g_return_val_if_fail (NAUTILUS_IS_TOOLBAR (self), FALSE);

    popover = GTK_POPOVER (gtk_menu_button_get_popover (GTK_MENU_BUTTON (self->view_button)));

    return gtk_widget_is_visible (GTK_WIDGET (popover));
}

gboolean
nautilus_toolbar_is_operations_button_active (NautilusToolbar *self)
{
    return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->operations_button));
}
