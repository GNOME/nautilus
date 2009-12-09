/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2009 Red Hat, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-view-as-action.h"
#include "nautilus-navigation-window.h"
#include "nautilus-window-private.h"
#include "nautilus-navigation-window-slot.h"
#include <gtk/gtk.h>
#include <eel/eel-gtk-extensions.h>
#include <libnautilus-private/nautilus-view-factory.h>

G_DEFINE_TYPE (NautilusViewAsAction, nautilus_view_as_action, GTK_TYPE_ACTION)

static void nautilus_view_as_action_init       (NautilusViewAsAction *action);
static void nautilus_view_as_action_class_init (NautilusViewAsActionClass *class);

static GObjectClass *parent_class = NULL;

#define NAUTILUS_VIEW_AS_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NAUTILUS_TYPE_VIEW_AS_ACTION, NautilusViewAsActionPrivate))

struct NautilusViewAsActionPrivate
{
	NautilusNavigationWindow *window;
};

enum
{
	PROP_0,
	PROP_WINDOW
};


static void
activate_nth_short_list_item (NautilusWindow *window, guint index)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = nautilus_window_get_active_slot (window);
	g_assert (index < g_list_length (window->details->short_list_viewers));

	nautilus_window_slot_set_content_view (slot,
					       g_list_nth_data (window->details->short_list_viewers, index));
}

static void
activate_extra_viewer (NautilusWindow *window)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = nautilus_window_get_active_slot (window);
	g_assert (window->details->extra_viewer != NULL);

	nautilus_window_slot_set_content_view (slot, window->details->extra_viewer);
}

static void
view_as_menu_switch_views_callback (GtkComboBox *combo_box, NautilusNavigationWindow *window)
{
	int active;

	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	active = gtk_combo_box_get_active (combo_box);

	if (active < 0) {
		return;
	} else if (active < GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo_box), "num viewers"))) {
		activate_nth_short_list_item (NAUTILUS_WINDOW (window), active);
	} else {
		activate_extra_viewer (NAUTILUS_WINDOW (window));
	}
}

static void
view_as_changed_callback (NautilusWindow *window,
			  GtkComboBox *combo_box)
{
	NautilusWindowSlot *slot;
	GList *node;
	int index;
	int selected_index = -1;
	GtkTreeModel *model;
	GtkListStore *store;
	const NautilusViewInfo *info;

	/* Clear the contents of ComboBox in a wacky way because there
	 * is no function to clear all items and also no function to obtain
	 * the number of items in a combobox.
	 */
	model = gtk_combo_box_get_model (combo_box);
	g_return_if_fail (GTK_IS_LIST_STORE (model));
	store = GTK_LIST_STORE (model);
	gtk_list_store_clear (store);

	slot = nautilus_window_get_active_slot (window);

	/* Add a menu item for each view in the preferred list for this location. */
	for (node = window->details->short_list_viewers, index = 0;
	     node != NULL;
	     node = node->next, ++index) {
		info = nautilus_view_factory_lookup (node->data);
		gtk_combo_box_append_text (combo_box, _(info->view_combo_label));

		if (nautilus_window_slot_content_view_matches_iid (slot, (char *)node->data)) {
			selected_index = index;
		}
	}
	g_object_set_data (G_OBJECT (combo_box), "num viewers", GINT_TO_POINTER (index));
	if (selected_index == -1) {
		const char *id;
		/* We're using an extra viewer, add a menu item for it */

		id = nautilus_window_slot_get_content_view_id (slot);
		info = nautilus_view_factory_lookup (id);
		gtk_combo_box_append_text (combo_box,
					   _(info->view_combo_label));
		selected_index = index;
	}

	gtk_combo_box_set_active (combo_box, selected_index);
}


static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy)) {
		GtkToolItem *item = GTK_TOOL_ITEM (proxy);
		NautilusViewAsAction *vaction = NAUTILUS_VIEW_AS_ACTION (action);
		NautilusNavigationWindow *window = vaction->priv->window;
		GtkWidget *view_as_menu_vbox;
		GtkWidget *view_as_combo_box;

		/* Option menu for content view types; it's empty here, filled in when a uri is set.
		 * Pack it into vbox so it doesn't grow vertically when location bar does.
		 */
		view_as_menu_vbox = gtk_vbox_new (FALSE, 4);
		gtk_widget_show (view_as_menu_vbox);

		gtk_container_set_border_width (GTK_CONTAINER (item), 4);
		gtk_container_add (GTK_CONTAINER (item), view_as_menu_vbox);

		view_as_combo_box = gtk_combo_box_new_text ();

		gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (view_as_combo_box), FALSE);
		gtk_box_pack_end (GTK_BOX (view_as_menu_vbox), view_as_combo_box, TRUE, FALSE, 0);
		gtk_widget_show (view_as_combo_box);
		g_signal_connect_object (view_as_combo_box, "changed",
					 G_CALLBACK (view_as_menu_switch_views_callback), window, 0);

		g_signal_connect (window, "view-as-changed",
				  G_CALLBACK (view_as_changed_callback),
				  view_as_combo_box);
	}

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
disconnect_proxy (GtkAction *action,
		  GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy)) {
		NautilusViewAsAction *vaction = NAUTILUS_VIEW_AS_ACTION (action);
		NautilusNavigationWindow *window = vaction->priv->window;

		g_signal_handlers_disconnect_matched (window,
						      G_SIGNAL_MATCH_FUNC,
						      0, 0, NULL, G_CALLBACK (view_as_changed_callback), NULL);
	}

	(* GTK_ACTION_CLASS (parent_class)->disconnect_proxy) (action, proxy);
}

static void
nautilus_view_as_action_finalize (GObject *object)
{
	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
nautilus_view_as_action_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	NautilusViewAsAction *zoom;

	zoom = NAUTILUS_VIEW_AS_ACTION (object);

	switch (prop_id) {
	case PROP_WINDOW:
		zoom->priv->window = NAUTILUS_NAVIGATION_WINDOW (g_value_get_object (value));
		break;
	}
}

static void
nautilus_view_as_action_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	NautilusViewAsAction *zoom;

	zoom = NAUTILUS_VIEW_AS_ACTION (object);

	switch (prop_id) {
	case PROP_WINDOW:
		g_value_set_object (value, zoom->priv->window);
		break;
	}
}

static void
nautilus_view_as_action_class_init (NautilusViewAsActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = nautilus_view_as_action_finalize;
	object_class->set_property = nautilus_view_as_action_set_property;
	object_class->get_property = nautilus_view_as_action_get_property;

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The navigation window",
							      G_TYPE_OBJECT,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(NautilusViewAsActionPrivate));
}

static void
nautilus_view_as_action_init (NautilusViewAsAction *action)
{
	action->priv = NAUTILUS_VIEW_AS_ACTION_GET_PRIVATE (action);
}
