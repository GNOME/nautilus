
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

#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-progress-info-manager.h>
#include <libnautilus-private/nautilus-file-operations.h>

#include <glib/gi18n.h>
#include <math.h>

#define OPERATION_MINIMUM_TIME 2 //s
#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 //ms
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 3 //s

typedef enum {
	NAUTILUS_NAVIGATION_DIRECTION_NONE,
	NAUTILUS_NAVIGATION_DIRECTION_BACK,
	NAUTILUS_NAVIGATION_DIRECTION_FORWARD
} NautilusNavigationDirection;

struct _NautilusToolbarPrivate {
	NautilusWindow *window;

	GtkWidget *path_bar_container;
	GtkWidget *location_entry_container;
	GtkWidget *path_bar;
	GtkWidget *location_entry;

	gboolean show_location_entry;

	guint popup_timeout_id;
        guint start_operations_timeout_id;
        guint remove_finished_operations_timeout_id;
        guint operations_button_attention_timeout_id;

	GtkWidget *operations_button;
	GtkWidget *view_button;
	GtkWidget *action_button;

        GtkWidget *operations_popover;
        GtkWidget *operations_container;
        GtkWidget *operations_revealer;
        GtkWidget *operations_icon;
	GtkWidget *view_icon;
	GMenu *action_menu;

	GtkWidget *forward_button;
	GtkWidget *back_button;

        NautilusProgressInfoManager *progress_manager;

        /* active slot & bindings */
        NautilusWindowSlot *active_slot;
        GBinding *icon_binding;
        GBinding *view_widget_binding;
};

enum {
	PROP_WINDOW = 1,
	PROP_SHOW_LOCATION_ENTRY,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE(NautilusToolbar, nautilus_toolbar, GTK_TYPE_HEADER_BAR);

static void unschedule_menu_popup_timeout (NautilusToolbar *self);
static void update_operations (NautilusToolbar *self);

static void
toolbar_update_appearance (NautilusToolbar *self)
{
	gboolean show_location_entry;

	show_location_entry = self->priv->show_location_entry ||
		g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

	gtk_widget_set_visible (self->priv->location_entry,
				show_location_entry);
	gtk_widget_set_visible (self->priv->path_bar,
				!show_location_entry);
}

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NautilusWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

	nautilus_window_back_or_forward (window, back, index, nautilus_event_get_window_open_flags ());
}

static void
activate_back_menu_item_callback (GtkMenuItem *menu_item,
                                  NautilusWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem *menu_item, NautilusWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static void
fill_menu (NautilusWindow *window,
	   GtkWidget *menu,
	   gboolean back)
{
	NautilusWindowSlot *slot;
	GtkWidget *menu_item;
	int index;
	GList *list;

	slot = nautilus_window_get_active_slot (window);
	list = back ? nautilus_window_slot_get_back_history (slot) :
		nautilus_window_slot_get_forward_history (slot);

	index = 0;
	while (list != NULL) {
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

/* adapted from gtk/gtkmenubutton.c */
static void
menu_position_func (GtkMenu       *menu,
		    gint          *x,
		    gint          *y,
		    gboolean      *push_in,
		    GtkWidget     *widget)
{
	GtkWidget *toplevel;
	GtkRequisition menu_req;
	GdkRectangle monitor;
	gint monitor_num;
	GdkScreen *screen;
	GdkWindow *window;
	GtkAllocation allocation;

	/* Set the dropdown menu hint on the toplevel, so the WM can omit the top side
	 * of the shadows.
	 */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu));
	gtk_window_set_type_hint (GTK_WINDOW (toplevel), GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU);

	window = gtk_widget_get_window (widget);
	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	if (monitor_num < 0) {
		monitor_num = 0;
	}

	gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_req, NULL);
	gtk_widget_get_allocation (widget, &allocation);
	gdk_window_get_origin (window, x, y);

	*x += allocation.x;
	*y += allocation.y;

	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
		*x -= MAX (menu_req.width - allocation.width, 0);
	} else {
		*x += MAX (allocation.width - menu_req.width, 0);
	}

	if ((*y + allocation.height + menu_req.height) <= monitor.y + monitor.height) {
		*y += allocation.height;
	} else if ((*y - menu_req.height) >= monitor.y) {
		*y -= menu_req.height;
	} else if (monitor.y + monitor.height - (*y + allocation.height) > *y) {
		*y += allocation.height;
	} else {
		*y -= menu_req.height;
	}

	*push_in = FALSE;
}

static void
show_menu (NautilusToolbar *self,
	   GtkWidget *widget,
           guint button,
           guint32 event_time)
{
	NautilusWindow *window;
	GtkWidget *menu;
	NautilusNavigationDirection direction;

	window = self->priv->window;
	menu = gtk_menu_new ();

	direction = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
							 "nav-direction"));

	switch (direction) {
	case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
		fill_menu (window, menu, FALSE);
		break;
	case NAUTILUS_NAVIGATION_DIRECTION_BACK:
		fill_menu (window, menu, TRUE);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			(GtkMenuPositionFunc) menu_position_func, widget,
                        button, event_time);
}

#define MENU_POPUP_TIMEOUT 1200

typedef struct {
	NautilusToolbar *self;
	GtkWidget *widget;
} ScheduleMenuData;

static void
schedule_menu_data_free (ScheduleMenuData *data)
{
	g_slice_free (ScheduleMenuData, data);
}

static gboolean
popup_menu_timeout_cb (gpointer user_data)
{
	ScheduleMenuData *data = user_data;

        show_menu (data->self, data->widget,
		   1, gtk_get_current_event_time ());

        return FALSE;
}

static void
unschedule_menu_popup_timeout (NautilusToolbar *self)
{
        if (self->priv->popup_timeout_id != 0) {
                g_source_remove (self->priv->popup_timeout_id);
                self->priv->popup_timeout_id = 0;
        }
}

static void
schedule_menu_popup_timeout (NautilusToolbar *self,
			     GtkWidget *widget)
{
	ScheduleMenuData *data;

        /* unschedule any previous timeouts */
        unschedule_menu_popup_timeout (self);

	data = g_slice_new0 (ScheduleMenuData);
	data->self = self;
	data->widget = widget;

        self->priv->popup_timeout_id =
                g_timeout_add_full (G_PRIORITY_DEFAULT, MENU_POPUP_TIMEOUT,
				    popup_menu_timeout_cb, data,
				    (GDestroyNotify) schedule_menu_data_free);
}
static gboolean
navigation_button_press_cb (GtkButton *button,
			    GdkEventButton *event,
			    gpointer user_data)
{
        NautilusToolbar *self = user_data;

        if (event->button == 3) {
                /* right click */
                show_menu (self, GTK_WIDGET (button), event->button, event->time);
                return TRUE;
        }

        if (event->button == 1) {
                schedule_menu_popup_timeout (self, GTK_WIDGET (button));
        }

	return FALSE;
}

static gboolean
navigation_button_release_cb (GtkButton *button,
                              GdkEventButton *event,
                              gpointer user_data)
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

static GList*
get_filtered_progress_infos (NautilusToolbar *self)
{
        GList *l;
        GList *filtered_progress_infos;
        GList *progress_infos;

        progress_infos = nautilus_progress_info_manager_get_all_infos (self->priv->progress_manager);
        filtered_progress_infos = NULL;

        for (l = progress_infos; l != NULL; l = l->next) {
                if (should_show_progress_info (l->data)) {
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

        for (l = progress_infos; l != NULL; l = l->next) {
                if (nautilus_progress_info_get_total_elapsed_time (l->data) +
                    nautilus_progress_info_get_remaining_time (l->data) > OPERATION_MINIMUM_TIME &&
                    !nautilus_progress_info_get_is_cancelled (l->data) &&
                    !nautilus_progress_info_get_is_finished (l->data)) {
                        return FALSE;
                }
        }

        g_list_free (progress_infos);

        return TRUE;
}

static gboolean
on_remove_finished_operations_timeout (NautilusToolbar *self)
{
        nautilus_progress_info_manager_remove_finished_or_cancelled_infos (self->priv->progress_manager);
        if (should_hide_operations_button (self)) {
                gtk_revealer_set_reveal_child (GTK_REVEALER (self->priv->operations_revealer),
                                               FALSE);
        } else {
                update_operations (self);
        }

        self->priv->remove_finished_operations_timeout_id = 0;

        return G_SOURCE_REMOVE;
}

static void
unschedule_remove_finished_operations (NautilusToolbar *self)
{
        if (self->priv->remove_finished_operations_timeout_id != 0) {
                g_source_remove (self->priv->remove_finished_operations_timeout_id);
                self->priv->remove_finished_operations_timeout_id = 0;
        }
}

static void
schedule_remove_finished_operations (NautilusToolbar *self)
{
        if (self->priv->remove_finished_operations_timeout_id == 0) {
                self->priv->remove_finished_operations_timeout_id =
                        g_timeout_add_seconds (REMOVE_FINISHED_OPERATIONS_TIEMOUT,
                                               (GSourceFunc) on_remove_finished_operations_timeout,
                                               self);
        }
}

static void
remove_operations_button_attention_style (NautilusToolbar *self)
{
        GtkStyleContext *style_context;

        style_context = gtk_widget_get_style_context (self->priv->operations_button);
        gtk_style_context_remove_class (style_context,
                                        "nautilus-operations-button-needs-attention");
}

static gboolean
on_remove_operations_button_attention_style_timeout (NautilusToolbar *self)
{
        remove_operations_button_attention_style (self);
        self->priv->operations_button_attention_timeout_id = 0;

        return G_SOURCE_REMOVE;
}

static void
unschedule_operations_button_attention_style (NautilusToolbar *self)
{
        if (self->priv->operations_button_attention_timeout_id!= 0) {
                g_source_remove (self->priv->operations_button_attention_timeout_id);
                self->priv->operations_button_attention_timeout_id = 0;
        }
}

static void
add_operations_button_attention_style (NautilusToolbar *self)
{
        GtkStyleContext *style_context;

        style_context = gtk_widget_get_style_context (self->priv->operations_button);

        unschedule_operations_button_attention_style (self);
        remove_operations_button_attention_style (self);

        gtk_style_context_add_class (style_context,
                                     "nautilus-operations-button-needs-attention");
        self->priv->operations_button_attention_timeout_id = g_timeout_add (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                                                                            (GSourceFunc) on_remove_operations_button_attention_style_timeout,
                                                                            self);
}

/* It's not the most beautiful solution, but we need to check wheter all windows
 * have it's button inactive, so the toolbar can schedule to remove the operations
 * only in that case to avoid other windows to show an empty popover in the oposite
 * case */
static gboolean
is_all_windows_operations_buttons_inactive ()
{
        GApplication *application;
        GList *windows;
        GList *l;
        GtkWidget *toolbar;

        application = g_application_get_default ();
        windows = nautilus_application_get_windows (NAUTILUS_APPLICATION (application));

        for (l = windows; l != NULL; l = l->next) {
                toolbar = nautilus_window_get_toolbar (NAUTILUS_WINDOW (l->data));
                if (nautilus_toolbar_is_operations_button_active (NAUTILUS_TOOLBAR (toolbar))) {
                          return FALSE;
                }
        }

        return TRUE;
}

static void
on_progress_info_cancelled (NautilusToolbar *self)
{
        /* Update the pie chart progress */
        gtk_widget_queue_draw (self->priv->operations_icon);

        if (is_all_windows_operations_buttons_inactive ()) {
                schedule_remove_finished_operations (self);
        }
}

static void
on_progress_info_progress_changed (NautilusToolbar *self)
{
        /* Update the pie chart progress */
        gtk_widget_queue_draw (self->priv->operations_icon);
}

static void
on_progress_info_finished (NautilusToolbar      *self,
                           NautilusProgressInfo *info)
{
        gchar *main_label;
        GFile *folder_to_open;

        /* Update the pie chart progress */
        gtk_widget_queue_draw (self->priv->operations_icon);

        if (is_all_windows_operations_buttons_inactive ()){
                schedule_remove_finished_operations (self);
        }

        folder_to_open = nautilus_progress_info_get_destination (info);
        /* If destination is null, don't show a notification. This happens when the
         * operation is a trash operation, which we already show a diferent kind of
         * notification */
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button)) &&
            folder_to_open != NULL) {
                add_operations_button_attention_style (self);
                main_label = nautilus_progress_info_get_status (info);
                nautilus_window_show_operation_notification (self->priv->window,
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

        progress_infos = nautilus_progress_info_manager_get_all_infos (self->priv->progress_manager);
        for (l = progress_infos; l != NULL; l = l->next) {
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

        gtk_container_foreach (GTK_CONTAINER (self->priv->operations_container),
                               (GtkCallback) gtk_widget_destroy,
                               NULL);

        disconnect_progress_infos (self);

        progress_infos = get_filtered_progress_infos (self);
        for (l = progress_infos; l != NULL; l = l->next) {
                should_show_progress_button = should_show_progress_button ||
                                              should_show_progress_info (l->data);

                g_signal_connect_swapped (l->data, "finished",
                                          G_CALLBACK (on_progress_info_finished), self);
                g_signal_connect_swapped (l->data, "cancelled",
                                          G_CALLBACK (on_progress_info_cancelled), self);
                g_signal_connect_swapped (l->data, "progress-changed",
                                          G_CALLBACK (on_progress_info_progress_changed), self);
                progress = nautilus_progress_info_widget_new (l->data);
                gtk_box_pack_start (GTK_BOX (self->priv->operations_container),
                                    progress,
                                    FALSE, FALSE, 0);
        }

        g_list_free (progress_infos);

        if (should_show_progress_button &&
            !gtk_revealer_get_reveal_child (GTK_REVEALER (self->priv->operations_revealer))) {
                add_operations_button_attention_style (self);
                gtk_revealer_set_reveal_child (GTK_REVEALER (self->priv->operations_revealer),
                                               TRUE);
                gtk_widget_queue_draw (self->priv->operations_icon);

                /* Show the popover at start to increase visibility.
                 * Check whether the toolbar is visible or not before showing the
                 * popover. This can happens if the window has the disables-chrome
                 * property set. */
                if (gtk_widget_get_visible (GTK_WIDGET (self))) {
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->operations_button),
                                                      TRUE);
                }
        }

        /* Since we removed the info widgets, we need to restore the focus */
        if (gtk_widget_get_visible (self->priv->operations_popover)) {
                gtk_widget_grab_focus (self->priv->operations_popover);
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

        progress_infos = nautilus_progress_info_manager_get_all_infos (self->priv->progress_manager);
        filtered_progress_infos = get_filtered_progress_infos (self);
        if (!nautilus_progress_manager_are_all_infos_finished_or_cancelled (self->priv->progress_manager) &&
            g_list_length (progress_infos) != g_list_length (filtered_progress_infos)) {
                g_list_free (filtered_progress_infos);
                return G_SOURCE_CONTINUE;
        } else {
                g_list_free (filtered_progress_infos);
                self->priv->start_operations_timeout_id = 0;
                return G_SOURCE_REMOVE;
        }
}

static void
schedule_operations_start (NautilusToolbar *self)
{
        if (self->priv->start_operations_timeout_id == 0) {
                /* Timeout is a little more than what we require for a stimated operation
                 * total time, to make sure the stimated total time is correct */
                self->priv->start_operations_timeout_id =
                        g_timeout_add (SECONDS_NEEDED_FOR_APROXIMATE_TRANSFER_RATE * 1000 + 500,
                                       (GSourceFunc) on_progress_info_started_timeout,
                                       self);
        }
}

static void
unschedule_operations_start (NautilusToolbar *self)
{
        if (self->priv->start_operations_timeout_id != 0) {
                g_source_remove (self->priv->start_operations_timeout_id);
                self->priv->start_operations_timeout_id = 0;
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
        for (l = progress_infos; l != NULL; l = l->next) {
                if (!nautilus_progress_info_get_is_cancelled (l->data)) {
                        all_cancelled = FALSE;
                        remaining_progress += nautilus_progress_info_get_remaining_time (l->data);
                        elapsed_progress += nautilus_progress_info_get_elapsed_time (l->data);
                }
        }

        g_list_free (progress_infos);

        total_progress = remaining_progress + elapsed_progress;

        if (all_cancelled) {
                ratio = 1.0;
        } else {
                if (total_progress > 0) {
                        ratio = MAX (0.05, elapsed_progress / total_progress);
                } else {
                        ratio = 0.05;
                }
        }


        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);

        gdk_cairo_set_source_rgba(cr, &background);
        cairo_arc (cr,
                   width / 2.0, height / 2.0,
                   MIN (width, height) / 2.0,
                   0, 2 *G_PI);
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
on_operations_button_toggled (NautilusToolbar *self)
{
        unschedule_remove_finished_operations (self);
        if (is_all_windows_operations_buttons_inactive ()) {
                schedule_remove_finished_operations (self);
        } else {
                update_operations (self);
        }
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
	GtkBuilder *builder;

	self->priv = nautilus_toolbar_get_instance_private (self);
	gtk_widget_init_template (GTK_WIDGET (self));

	self->priv->path_bar = g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL);
	gtk_container_add (GTK_CONTAINER (self->priv->path_bar_container),
					  self->priv->path_bar);

	self->priv->location_entry = nautilus_location_entry_new ();
	gtk_container_add (GTK_CONTAINER (self->priv->location_entry_container),
					  self->priv->location_entry);

	builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-toolbar-action-menu.ui");
	self->priv->action_menu = G_MENU (gtk_builder_get_object (builder, "action-menu"));
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->priv->action_button),
					G_MENU_MODEL (self->priv->action_menu));
	g_object_unref (builder);

        self->priv->progress_manager = nautilus_progress_info_manager_dup_singleton ();
	g_signal_connect (self->priv->progress_manager, "new-progress-info",
			  G_CALLBACK (on_new_progress_info), self);
        update_operations (self);

	g_object_set_data (G_OBJECT (self->priv->back_button), "nav-direction",
			   GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_BACK));
	g_object_set_data (G_OBJECT (self->priv->forward_button), "nav-direction",
			   GUINT_TO_POINTER (NAUTILUS_NAVIGATION_DIRECTION_FORWARD));
	g_signal_connect (self->priv->back_button, "button-press-event",
			  G_CALLBACK (navigation_button_press_cb), self);
	g_signal_connect (self->priv->back_button, "button-release-event",
			  G_CALLBACK (navigation_button_release_cb), self);
	g_signal_connect (self->priv->forward_button, "button-press-event",
			  G_CALLBACK (navigation_button_press_cb), self);
	g_signal_connect (self->priv->forward_button, "button-release-event",
			  G_CALLBACK (navigation_button_release_cb), self);
        g_signal_connect (self->priv->operations_popover, "show",
                          (GCallback) gtk_widget_grab_focus, NULL);
        g_signal_connect_swapped (self->priv->operations_popover, "closed",
                                  (GCallback) gtk_widget_grab_focus, self);

	gtk_widget_show_all (GTK_WIDGET (self));
	toolbar_update_appearance (self);
}

static void
nautilus_toolbar_get_property (GObject *object,
			       guint property_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

	switch (property_id) {
	case PROP_SHOW_LOCATION_ENTRY:
		g_value_set_boolean (value, self->priv->show_location_entry);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_toolbar_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

	switch (property_id) {
	case PROP_WINDOW:
		self->priv->window = g_value_get_object (value);
		break;
	case PROP_SHOW_LOCATION_ENTRY:
		nautilus_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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

        g_signal_handlers_disconnect_by_data (self->priv->progress_manager, self);
        g_clear_object (&self->priv->progress_manager);

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

	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_icon);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_popover);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_container);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_revealer);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, view_button);
        gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, view_icon);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, action_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, path_bar_container);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, location_entry_container);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, back_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, forward_button);

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

GMenu *
nautilus_toolbar_get_action_menu (NautilusToolbar *self)
{
	return self->priv->action_menu;
}

GtkWidget *
nautilus_toolbar_get_path_bar (NautilusToolbar *self)
{
	return self->priv->path_bar;
}

GtkWidget *
nautilus_toolbar_get_location_entry (NautilusToolbar *self)
{
	return self->priv->location_entry;
}

void
nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
					  gboolean show_location_entry)
{
	if (show_location_entry != self->priv->show_location_entry) {
		self->priv->show_location_entry = show_location_entry;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LOCATION_ENTRY]);
	}
}

static gboolean
nautilus_toolbar_view_icon_transform_to (GBinding     *binding,
                                         const GValue *from_value,
                                         GValue       *to_value,
                                         gpointer      user_data)
{
        GIcon *icon;

        icon = g_value_get_object (from_value);

        /* As per design decision, we let the previous used icon if no
         * view menu is available */
        if (icon) {
                g_value_set_object (to_value, icon);
        }

        return TRUE;
}

static gboolean
nautilus_toolbar_view_widget_transform_to (GBinding     *binding,
                                           const GValue *from_value,
                                           GValue       *to_value,
                                           gpointer      user_data)
{
        NautilusToolbar *toolbar;
        GtkWidget *view_widget;

        toolbar = NAUTILUS_TOOLBAR (user_data);
        view_widget = g_value_get_object (from_value);

        gtk_widget_set_sensitive (toolbar->priv->view_button, view_widget != NULL);
        gtk_menu_button_set_popover (GTK_MENU_BUTTON (toolbar->priv->view_button), NULL);

        g_value_set_object (to_value, view_widget);

        return TRUE;
}

void
nautilus_toolbar_set_active_slot (NautilusToolbar    *toolbar,
                                  NautilusWindowSlot *slot)
{
        g_return_if_fail (NAUTILUS_IS_TOOLBAR (toolbar));

        g_clear_pointer (&toolbar->priv->icon_binding, g_binding_unbind);
        g_clear_pointer (&toolbar->priv->view_widget_binding, g_binding_unbind);

        if (toolbar->priv->active_slot != slot) {
                toolbar->priv->active_slot = slot;

                if (slot) {
                        toolbar->priv->icon_binding =
                                        g_object_bind_property_full (slot, "icon",
                                                                     toolbar->priv->view_icon, "gicon",
                                                                     G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                                                     (GBindingTransformFunc) nautilus_toolbar_view_icon_transform_to,
                                                                     NULL,
                                                                     toolbar,
                                                                     NULL);
                        toolbar->priv->view_widget_binding =
                                        g_object_bind_property_full (slot, "view-widget",
                                                                     toolbar->priv->view_button, "popover",
                                                                     G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                                                                     (GBindingTransformFunc) nautilus_toolbar_view_widget_transform_to,
                                                                     NULL,
                                                                     toolbar,
                                                                     NULL);
                }

        }
}

gboolean
nautilus_toolbar_is_operations_button_active (NautilusToolbar *self)
{
        return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button));
}
