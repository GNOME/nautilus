/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com> 
 */

/* nautilus-window-toolbars.c - implementation of nautilus window toolbar operations,
 * split into separate file just for convenience.
 */

#include <config.h>

#include <unistd.h>
#include "nautilus-application.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktogglebutton.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-theme.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

#define TOOLBAR_PATH_EXTENSION_ACTIONS "/Toolbar/Extra Buttons Placeholder/Extension Actions"

enum {
	TOOLBAR_ITEM_STYLE_PROP,
	TOOLBAR_ITEM_ORIENTATION_PROP
};

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

static GtkMenu *
create_back_or_forward_menu (NautilusNavigationWindow *window, gboolean back)
{
	GtkMenu *menu;
	GtkWidget *menu_item;
	GList *list_link;
	int index;

	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));
	
	menu = GTK_MENU (gtk_menu_new ());

	list_link = back ? window->back_list : window->forward_list;
	index = 0;
	while (list_link != NULL)
	{
		menu_item = nautilus_bookmark_menu_item_new (NAUTILUS_BOOKMARK (list_link->data));		
		g_object_set_data (G_OBJECT (menu_item), "user_data", GINT_TO_POINTER (index));
		gtk_widget_show (GTK_WIDGET (menu_item));
  		g_signal_connect_object (menu_item, "activate",
					 back
					 ? G_CALLBACK (activate_back_menu_item_callback)
					 : G_CALLBACK (activate_forward_menu_item_callback),
					 window, 0);
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		list_link = g_list_next (list_link);
		++index;
	}

	return menu;
}

static GtkWidget *
get_back_button (NautilusNavigationWindow *window)
{
	return window->details->back_button_item;
}

static GtkWidget *
get_forward_button (NautilusNavigationWindow *window)
{
	return window->details->forward_button_item;
}

static void
menu_position_under_widget (GtkMenu *menu, int *x, int *y,
			    gboolean *push_in, gpointer user_data)
{
	GtkWidget *w;
	int screen_width, screen_height;
	GtkRequisition requisition;

	w = GTK_WIDGET (user_data);
	
	gdk_window_get_origin (w->window, x, y);
	*x += w->allocation.x;
	*y += w->allocation.y + w->allocation.height;

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	*x = CLAMP (*x, 0, MAX (0, screen_width - requisition.width));
	*y = CLAMP (*y, 0, MAX (0, screen_height - requisition.height));
}

static gboolean
back_or_forward_button_pressed_callback (GtkWidget *widget, 
					 GdkEventButton *event,
					 gpointer *user_data)
{
	NautilusNavigationWindow *window;
	gboolean back;

	g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (user_data), FALSE);

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	back = widget == get_back_button (window);
	g_assert (back || widget == get_forward_button (window));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	gnome_popup_menu_do_popup_modal (GTK_WIDGET (create_back_or_forward_menu (NAUTILUS_NAVIGATION_WINDOW (user_data), back)),
					 menu_position_under_widget, widget, event, widget, widget);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	
	return TRUE;
}

static gboolean
back_or_forward_key_pressed_callback (GtkWidget *widget,
				      GdkEventKey *event,
				      gpointer *user_data)
{
	if (event->keyval == GDK_space ||
	    event->keyval == GDK_KP_Space ||
	    event->keyval == GDK_Return ||
	    event->keyval == GDK_KP_Enter) {
		back_or_forward_button_pressed_callback (widget, NULL, user_data);
	}

	return FALSE;
}

static GtkWidget *
create_back_or_forward_toolbar_item (NautilusNavigationWindow *window, 
				     const char     *tooltip,
				     const char     *control_path)
{
	GtkWidget *button;

	button = gtk_toggle_button_new ();

	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

	gtk_container_add (GTK_CONTAINER (button),
			   gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT));

	gtk_widget_show_all (GTK_WIDGET (button));

	gtk_tooltips_set_tip (window->details->tooltips,
			      GTK_WIDGET (button),
			      tooltip, NULL);

	g_signal_connect_object (button, "key_press_event",
				 G_CALLBACK (back_or_forward_key_pressed_callback),
				 window, 0);
	g_signal_connect_object (button, "button_press_event",
				 G_CALLBACK (back_or_forward_button_pressed_callback),
				 window, 0);

	bonobo_ui_component_widget_set (NAUTILUS_WINDOW (window)->details->shell_ui,
					control_path,
					button,
					NULL);

	return button;
}

static void
throbber_set_throbbing (NautilusNavigationWindow *window,
			gboolean        throbbing)
{
	CORBA_boolean b;
	CORBA_any     val;

	val._type = TC_CORBA_boolean;
	val._value = &b;
	b = throbbing;

	bonobo_pbclient_set_value_async (
		window->details->throbber_property_bag,
		"throbbing", &val, NULL);
}

static void
throbber_created_callback (Bonobo_Unknown     throbber,
			   CORBA_Environment *ev,
			   gpointer           user_data)
{
	char *exception_as_text;
	NautilusNavigationWindow *window;

	if (BONOBO_EX (ev)) {
		exception_as_text = bonobo_exception_get_text (ev);
		g_warning ("Throbber activation exception '%s'", exception_as_text);
		g_free (exception_as_text);
		return;
	}

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (user_data));

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	window->details->throbber_activating = FALSE;

	bonobo_ui_component_object_set (NAUTILUS_WINDOW (window)->details->shell_ui,
					"/Toolbar/ThrobberWrapper",
					throbber, ev);
	CORBA_exception_free (ev);

	window->details->throbber_property_bag =
		Bonobo_Control_getProperties (throbber, ev);

	if (BONOBO_EX (ev)) {
		window->details->throbber_property_bag = CORBA_OBJECT_NIL;
		CORBA_exception_free (ev);
	} else {
		throbber_set_throbbing (window, window->details->throbber_active);
	}

	bonobo_object_release_unref (throbber, ev);

	g_object_unref (window);
}

void
nautilus_navigation_window_set_throbber_active (NautilusNavigationWindow *window, 
						gboolean allow)
{
	if (( window->details->throbber_active &&  allow) ||
	    (!window->details->throbber_active && !allow)) {
		return;
	}

	if (allow)
		access ("nautilus-throbber: start", 0);
	else
		access ("nautilus-throbber: stop", 0);

	nautilus_bonobo_set_sensitive (NAUTILUS_WINDOW (window)->details->shell_ui,
				       NAUTILUS_COMMAND_STOP, allow);

	window->details->throbber_active = allow;
	if (window->details->throbber_property_bag != CORBA_OBJECT_NIL) {
		throbber_set_throbbing (window, allow);
	}
}

void
nautilus_navigation_window_activate_throbber (NautilusNavigationWindow *window)
{
	CORBA_Environment ev;
	char *exception_as_text;

	if (window->details->throbber_activating ||
	    window->details->throbber_property_bag != CORBA_OBJECT_NIL) {
		return;
	}

	/* FIXME bugzilla.gnome.org 41243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		CORBA_exception_init (&ev);

		g_object_ref (window);
		bonobo_get_object_async ("OAFIID:Nautilus_Throbber",
					 "IDL:Bonobo/Control:1.0",
					 &ev,
					 throbber_created_callback,
					 window);

		if (BONOBO_EX (&ev)) {
			exception_as_text = bonobo_exception_get_text (&ev);
			g_warning ("Throbber activation exception '%s'", exception_as_text);
			g_free (exception_as_text);
		}
		CORBA_exception_free (&ev);
		window->details->throbber_activating = TRUE;		
	}
}

void
nautilus_navigation_window_initialize_toolbars (NautilusNavigationWindow *window)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_navigation_window_activate_throbber (window);

	window->details->back_button_item = create_back_or_forward_toolbar_item 
		(window, _("Go back a few pages"),
		 "/Toolbar/BackMenu");
	window->details->forward_button_item = create_back_or_forward_toolbar_item 
		(window, _("Go forward a number of pages"),
		 "/Toolbar/ForwardMenu");

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

static GList *
get_extension_toolbar_items (NautilusNavigationWindow *window)
{
	GList *items;
	GList *providers;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;
		
		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_toolbar_items 
			(provider, 
			 GTK_WIDGET (window),
			 NAUTILUS_WINDOW (window)->details->viewed_file);
		items = g_list_concat (items, file_items);		
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

void
nautilus_navigation_window_load_extension_toolbar_items (NautilusNavigationWindow *window)
{
	GList *items;
	GList *l;
	
	nautilus_bonobo_remove_menu_items_and_commands
		(NAUTILUS_WINDOW (window)->details->shell_ui, 
		 TOOLBAR_PATH_EXTENSION_ACTIONS);

	items = get_extension_toolbar_items (window);

	for (l = items; l != NULL; l = l->next) {
		NautilusMenuItem *item;
		
		item = NAUTILUS_MENU_ITEM (l->data);

		nautilus_bonobo_add_extension_item_command
			(NAUTILUS_WINDOW (window)->details->shell_ui, item);
		
		nautilus_bonobo_add_extension_toolbar_item
			(NAUTILUS_WINDOW (window)->details->shell_ui, 
			 TOOLBAR_PATH_EXTENSION_ACTIONS,
			 item);

		g_object_unref (item);
	}

	g_list_free (items);
}
