/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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

#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-progress-info-manager.h>
#include <libnautilus-private/nautilus-file-operations.h>

#include <glib/gi18n.h>
#include <math.h>

#define OPERATION_MINIMUM_TIME 5 //s
#define REMOVE_FINISHED_OPERATIONS_TIEMOUT 2 //s

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

	GtkWidget *operations_button;
	GtkWidget *view_button;
	GtkWidget *action_button;

        GtkWidget *operations_popover;
        GtkWidget *operations_container;
        GtkWidget *operations_revealer;
        GtkWidget *operations_icon;
	GtkWidget *view_menu_widget;
	GtkWidget *sort_menu;
	GtkWidget *sort_modification_date;
	GtkWidget *sort_access_date;
	GtkWidget *sort_trash_time;
	GtkWidget *sort_search_relevance;
	GtkWidget *visible_columns;
	GtkWidget *stop;
	GtkWidget *reload;
	GtkAdjustment *zoom_adjustment;
	GtkWidget *zoom_level_scale;
	GMenu *action_menu;

	GtkWidget *forward_button;
	GtkWidget *back_button;

        NautilusProgressInfoManager *progress_manager;
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

static void
action_view_mode_state_changed (GActionGroup *action_group,
				gchar *action_name,
				GVariant *value,
				gpointer user_data)
{
	NautilusToolbar *self = user_data;
	const gchar *view_mode = g_variant_get_string (value, NULL);
	const gchar *name;
	GtkWidget *image;

	if (g_strcmp0 (view_mode, "list") == 0) {
		name = "view-list-symbolic";
	} else if (g_strcmp0 (view_mode, "grid") == 0) {
		name = "view-grid-symbolic";
	} else {
		g_assert_not_reached ();
	}

	image = gtk_image_new ();
	gtk_button_set_image (GTK_BUTTON (self->priv->view_button), image);
	gtk_image_set_from_icon_name (GTK_IMAGE (image), name,
				      GTK_ICON_SIZE_MENU);
}

static void
action_reload_enabled_changed (GActionGroup *action_group,
			       gchar *action_name,
			       gboolean enabled,
			       gpointer user_data)
{
	NautilusToolbar *self = user_data;
	gtk_widget_set_visible (self->priv->reload, enabled);
}

static void
action_stop_enabled_changed (GActionGroup *action_group,
			     gchar *action_name,
			     gboolean enabled,
			     gpointer user_data)
{
	NautilusToolbar *self = user_data;
	gtk_widget_set_visible (self->priv->stop, enabled);
}

static void
nautilus_toolbar_set_window (NautilusToolbar *self,
			     NautilusWindow *window)

{
	self->priv->window = window;

	g_signal_connect (self->priv->window, "action-enabled-changed::stop",
			  G_CALLBACK (action_stop_enabled_changed), self);
	g_signal_connect (self->priv->window, "action-enabled-changed::reload",
			  G_CALLBACK (action_reload_enabled_changed), self);
	g_signal_connect (self->priv->window, "action-state-changed::view-mode",
			  G_CALLBACK (action_view_mode_state_changed), self);
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

static void
zoom_level_changed (GtkRange *range,
		    NautilusToolbar *self)
{
	NautilusWindowSlot *slot;
	NautilusView *view;
	gdouble zoom_level;

	zoom_level = gtk_range_get_value (range);
	slot = nautilus_window_get_active_slot (self->priv->window);
	view = nautilus_window_slot_get_current_view (slot);

	g_action_group_change_action_state (nautilus_view_get_action_group (view),
					    "zoom-to-level",
					    g_variant_new_int32 ((gint) zoom_level));
}

static void
view_menu_popover_closed (GtkPopover *popover,
			  NautilusToolbar *self)
{
	NautilusWindowSlot *slot;
	NautilusView *view;

	slot = nautilus_window_get_active_slot (self->priv->window);
	view = nautilus_window_slot_get_current_view (slot);

	nautilus_view_grab_focus (view);
}

static gboolean
should_hide_operations_button (NautilusToolbar *self)
{
        GList *progress_infos;
        GList *l;

        progress_infos = nautilus_progress_info_manager_get_all_infos (self->priv->progress_manager);

        for (l = progress_infos; l != NULL; l = l->next) {
                if (nautilus_progress_info_get_elapsed_time (l->data) +
                    nautilus_progress_info_get_remaining_time (l->data) > OPERATION_MINIMUM_TIME &&
                    !nautilus_progress_info_get_is_cancelled (l->data) &&
                    !nautilus_progress_info_get_is_finished (l->data)) {
                        return FALSE;
                }
        }

        return TRUE;
}

static gboolean
remove_finished_operations (NautilusToolbar *self)
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
                                               (GSourceFunc) remove_finished_operations,
                                               self);
        }
}

static void
on_progress_info_cancelled (NautilusToolbar *self)
{
        /* Update the pie chart progress */
        gtk_widget_queue_draw (self->priv->operations_icon);
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button))) {
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
on_progress_info_finished (NautilusToolbar *self)
{
        /* Update the pie chart progress */
        gtk_widget_queue_draw (self->priv->operations_icon);

        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button))) {
                schedule_remove_finished_operations (self);
        }
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
        guint total_remaining_time = 0;

        gtk_container_foreach (GTK_CONTAINER (self->priv->operations_container),
                               (GtkCallback) gtk_widget_destroy,
                               NULL);

        disconnect_progress_infos (self);

        progress_infos = nautilus_progress_info_manager_get_all_infos (self->priv->progress_manager);
        for (l = progress_infos; l != NULL; l = l->next) {
                if (nautilus_progress_info_get_elapsed_time (l->data) +
                    nautilus_progress_info_get_remaining_time (l->data) > OPERATION_MINIMUM_TIME) {
                        total_remaining_time = nautilus_progress_info_get_remaining_time (l->data);

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
        }

        /* Either we are already showing the button, so keep showing it until the user
         * toggle it to hide the operations popover, or, if we want now to show it,
         * we have to have at least one operation that its total stimated time
         * is longer than OPERATION_MINIMUM_TIME seconds, or if we failed to get
         * a correct stimated time and it's around OPERATION_MINIMUM_TIME,
         * showing the button for just for a moment because now we realized the
         * estimated time is longer than a OPERATION_MINIMUM_TIME is odd, so show
         * it only if the remaining time is bigger than again OPERATION_MINIMUM_TIME.
         */
        if (total_remaining_time > OPERATION_MINIMUM_TIME ||
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button))) {
                gtk_revealer_set_reveal_child (GTK_REVEALER (self->priv->operations_revealer),
                                               TRUE);
                gtk_widget_queue_draw (self->priv->operations_icon);
        }
}

static gboolean
on_progress_info_started_timeout (NautilusToolbar *self)
{
        update_operations (self);

        /* In case we didn't show the operations button because the operation total
         * time stimation is not good enough, update again to make sure we don't miss
         * a long time operation because of that */
        if (!nautilus_progress_manager_are_all_infos_finished_or_cancelled (self->priv->progress_manager)) {
                return G_SOURCE_CONTINUE;
        } else {
                self->priv->start_operations_timeout_id = 0;
                if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button))) {
                        schedule_remove_finished_operations (self);
                }
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
        GdkRGBA background = {.red = 0, .green = 0, .blue = 0, .alpha = 0.2 };
        GdkRGBA foreground = {.red = 0, .green = 0, .blue = 0, .alpha = 0.7 };

        all_cancelled = TRUE;
        progress_infos = nautilus_progress_info_manager_get_all_infos (self->priv->progress_manager);
        for (l = progress_infos; l != NULL; l = l->next) {
                if (!nautilus_progress_info_get_is_cancelled (l->data)) {
                        all_cancelled = FALSE;
                        remaining_progress += nautilus_progress_info_get_remaining_time (l->data);
                        elapsed_progress += nautilus_progress_info_get_elapsed_time (l->data);
                }
        }

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
        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->operations_button))) {
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

	builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/nautilus-toolbar-view-menu.xml");
	self->priv->view_menu_widget =  GTK_WIDGET (gtk_builder_get_object (builder, "view_menu_widget"));
	self->priv->zoom_level_scale = GTK_WIDGET (gtk_builder_get_object (builder, "zoom_level_scale"));
	self->priv->zoom_adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "zoom_adjustment"));

	self->priv->sort_menu =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_menu"));
	self->priv->sort_modification_date =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_modification_date"));
	self->priv->sort_access_date =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_access_date"));
	self->priv->sort_trash_time =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_trash_time"));
	self->priv->sort_search_relevance =  GTK_WIDGET (gtk_builder_get_object (builder, "sort_search_relevance"));
	self->priv->visible_columns =  GTK_WIDGET (gtk_builder_get_object (builder, "visible_columns"));
	self->priv->reload =  GTK_WIDGET (gtk_builder_get_object (builder, "reload"));
	self->priv->stop =  GTK_WIDGET (gtk_builder_get_object (builder, "stop"));

	g_signal_connect (self->priv->view_menu_widget, "closed",
			  G_CALLBACK (view_menu_popover_closed), self);
	gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->priv->view_button),
				     self->priv->view_menu_widget);
	g_object_unref (builder);

	builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/nautilus-toolbar-action-menu.xml");
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
	g_signal_connect (self->priv->zoom_level_scale, "value-changed",
			  G_CALLBACK (zoom_level_changed), self);

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
		nautilus_toolbar_set_window (self, g_value_get_object (value));
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
nautilus_toolbar_dispose (GObject *obj)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      toolbar_update_appearance, self);
        disconnect_progress_infos (self);
	unschedule_menu_popup_timeout (self);
        unschedule_remove_finished_operations (self);
        unschedule_operations_start (self);

        g_signal_handlers_disconnect_by_data (self->priv->progress_manager, self);
        g_clear_object (&self->priv->progress_manager);

	G_OBJECT_CLASS (nautilus_toolbar_parent_class)->dispose (obj);
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
						     "/org/gnome/nautilus/nautilus-toolbar-ui.xml");

	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_icon);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_container);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, operations_revealer);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, view_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, action_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, path_bar_container);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, location_entry_container);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, back_button);
	gtk_widget_class_bind_template_child_private (widget_class, NautilusToolbar, forward_button);

        gtk_widget_class_bind_template_callback (widget_class, on_operations_icon_draw);
        gtk_widget_class_bind_template_callback (widget_class, on_operations_button_toggled);
}

void
nautilus_toolbar_reset_menus (NautilusToolbar *self)
{
	NautilusWindowSlot *slot;
	NautilusView *view;
	GActionGroup *view_action_group;
	GVariant *variant;
	GVariantIter iter;
	gboolean show_sort_trash, show_sort_search, show_sort_access, show_sort_modification, has_sort;
	const gchar *hint;

	/* Allow actions from the current view to be activated through
	 * the view menu and action menu of the toolbar */
	slot = nautilus_window_get_active_slot (self->priv->window);
	view = nautilus_window_slot_get_current_view (slot);
	view_action_group = nautilus_view_get_action_group (view);
	gtk_widget_insert_action_group (GTK_WIDGET (self),
					"view",
					G_ACTION_GROUP (view_action_group));

	gtk_widget_set_visible (self->priv->visible_columns,
				g_action_group_has_action (view_action_group, "visible-columns"));

	has_sort = g_action_group_has_action (view_action_group, "sort");
	show_sort_trash = show_sort_search = show_sort_modification = show_sort_access = FALSE;
	gtk_widget_set_visible (self->priv->sort_menu, has_sort);

	if (has_sort) {
		variant = g_action_group_get_action_state_hint (view_action_group, "sort");
		g_variant_iter_init (&iter, variant);

		while (g_variant_iter_next (&iter, "&s", &hint)) {
			if (g_strcmp0 (hint, "trash-time") == 0)
				show_sort_trash = TRUE;
			if (g_strcmp0 (hint, "search-relevance") == 0)
				show_sort_search = TRUE;
			if (g_strcmp0 (hint, "access-date") == 0)
				show_sort_access = TRUE;
			if (g_strcmp0 (hint, "modification-date") == 0)
				show_sort_modification = TRUE;
		}

		g_variant_unref (variant);
	}

	gtk_widget_set_visible (self->priv->sort_trash_time, show_sort_trash);
	gtk_widget_set_visible (self->priv->sort_search_relevance, show_sort_search);
	gtk_widget_set_visible (self->priv->sort_modification_date, show_sort_modification);
	gtk_widget_set_visible (self->priv->sort_access_date, show_sort_access);

	variant = g_action_group_get_action_state (view_action_group, "zoom-to-level");
	gtk_adjustment_set_value (self->priv->zoom_adjustment,
				  g_variant_get_int32 (variant));
	g_variant_unref (variant);
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
