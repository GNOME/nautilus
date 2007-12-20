/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *
 */

#include <config.h>

#include "nautilus-navigation-action.h"
#include "nautilus-navigation-window.h"

#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtkmenutoolbutton.h>

static void nautilus_navigation_action_init       (NautilusNavigationAction *action);
static void nautilus_navigation_action_class_init (NautilusNavigationActionClass *class);

static GObjectClass *parent_class = NULL;

#define NAUTILUS_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NAUTILUS_TYPE_NAVIGATION_ACTION, NautilusNavigationActionPrivate))

struct NautilusNavigationActionPrivate
{
	NautilusNavigationWindow *window;
	NautilusNavigationDirection direction;
	char *arrow_tooltip;
};

enum
{
	PROP_0,
	PROP_ARROW_TOOLTIP,
	PROP_DIRECTION,
	PROP_WINDOW
};

GType
nautilus_navigation_action_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		const GTypeInfo type_info = {
			sizeof (NautilusNavigationActionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) nautilus_navigation_action_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (NautilusNavigationAction),
			0, /* n_preallocs */
			(GInstanceInitFunc) nautilus_navigation_action_init,
		};
		
		type = g_type_register_static (GTK_TYPE_ACTION,
					       "NautilusNavigationAction",
					       &type_info, 0);
	}

	return type;
}

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NautilusNavigationWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

	nautilus_navigation_window_back_or_forward (window, back, index);
}

static void
activate_back_menu_item_callback (GtkMenuItem *menu_item, NautilusNavigationWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem *menu_item, NautilusNavigationWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static void
fill_menu (NautilusNavigationWindow *window,
	   GtkWidget *menu,
	   gboolean back)
{
	GtkWidget *menu_item;
	int index;
	GList *list;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	
	list = back ? window->back_list : window->forward_list;
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


static void
show_menu_callback (GtkMenuToolButton *button,
		    NautilusNavigationAction *action)
{
	NautilusNavigationActionPrivate *p;
	NautilusNavigationWindow *window;
	GtkWidget *menu;
	GList *children;
	GList *li;

	p = action->priv;
	window = action->priv->window;
	
	menu = gtk_menu_tool_button_get_menu (button);
	
	children = gtk_container_get_children (GTK_CONTAINER (menu));
	for (li = children; li; li = li->next) {
		gtk_container_remove (GTK_CONTAINER (menu), li->data);
	}
	g_list_free (children);

	switch (p->direction) {
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
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (GTK_IS_MENU_TOOL_BUTTON (proxy)) {
		NautilusNavigationAction *naction = NAUTILUS_NAVIGATION_ACTION (action);
		GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (proxy);
		GtkWidget *menu;

		/* set an empty menu, so the arrow button becomes sensitive */
		menu = gtk_menu_new ();
		gtk_menu_tool_button_set_menu (button, menu);

		gtk_menu_tool_button_set_arrow_tooltip_text (button,
							     naction->priv->arrow_tooltip);
		
		g_signal_connect (proxy, "show-menu",
				  G_CALLBACK (show_menu_callback), action);
	}

	(* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
}

static void
disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (GTK_IS_MENU_TOOL_BUTTON (proxy)) {
		g_signal_handlers_disconnect_by_func (proxy, G_CALLBACK (show_menu_callback), action);
	}

	(* GTK_ACTION_CLASS (parent_class)->disconnect_proxy) (action, proxy);
}

static void
nautilus_navigation_action_finalize (GObject *object)
{
	NautilusNavigationAction *action = NAUTILUS_NAVIGATION_ACTION (object);

	g_free (action->priv->arrow_tooltip);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
nautilus_navigation_action_set_property (GObject *object,
					 guint prop_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
	NautilusNavigationAction *nav;

	nav = NAUTILUS_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_free (nav->priv->arrow_tooltip);
			nav->priv->arrow_tooltip = g_value_dup_string (value);
			break;
		case PROP_DIRECTION:
			nav->priv->direction = g_value_get_int (value);
			break;
		case PROP_WINDOW:
			nav->priv->window = NAUTILUS_NAVIGATION_WINDOW (g_value_get_object (value));
			break;
	}
}

static void
nautilus_navigation_action_get_property (GObject *object,
					 guint prop_id,
					 GValue *value,
					 GParamSpec *pspec)
{
	NautilusNavigationAction *nav;

	nav = NAUTILUS_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_value_set_string (value, nav->priv->arrow_tooltip);
			break;
		case PROP_DIRECTION:
			g_value_set_int (value, nav->priv->direction);
			break;
		case PROP_WINDOW:
			g_value_set_object (value, nav->priv->window);
			break;
	}
}

static void
nautilus_navigation_action_class_init (NautilusNavigationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = nautilus_navigation_action_finalize;
	object_class->set_property = nautilus_navigation_action_set_property;
	object_class->get_property = nautilus_navigation_action_get_property;

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;

	g_object_class_install_property (object_class,
                                         PROP_ARROW_TOOLTIP,
                                         g_param_spec_string ("arrow-tooltip",
                                                              "Arrow Tooltip",
                                                              "Arrow Tooltip",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_DIRECTION,
                                         g_param_spec_int ("direction",
                                                           "Direction",
                                                           "Direction",
                                                           0,
							   G_MAXINT,
							   0,
                                                           G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "The navigation window",
                                                              G_TYPE_OBJECT,
                                                              G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(NautilusNavigationActionPrivate));
}

static void
nautilus_navigation_action_init (NautilusNavigationAction *action)
{
        action->priv = NAUTILUS_NAVIGATION_ACTION_GET_PRIVATE (action);
}
