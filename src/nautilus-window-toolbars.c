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
#include <libgnome/gnome-i18n.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-theme.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

enum {
	TOOLBAR_ITEM_STYLE_PROP,
	TOOLBAR_ITEM_ORIENTATION_PROP
};

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NautilusWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (NAUTILUS_IS_WINDOW (window));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));
	nautilus_window_back_or_forward (window, back, index);
}

static void
activate_back_menu_item_callback (GtkMenuItem *menu_item, NautilusWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem *menu_item, NautilusWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static GtkMenu *
create_back_or_forward_menu (NautilusWindow *window, gboolean back)
{
	GtkMenu *menu;
	GtkWidget *menu_item;
	GList *list_link;
	int index;

	g_assert (NAUTILUS_IS_WINDOW (window));
	
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
get_back_button (NautilusWindow *window)
{
	return GTK_WIDGET (bonobo_ui_toolbar_button_item_get_button_widget 
		(window->details->back_button_item));
}

static GtkWidget *
get_forward_button (NautilusWindow *window)
{
	return GTK_WIDGET (bonobo_ui_toolbar_button_item_get_button_widget 
		(window->details->forward_button_item));
}

static int
back_or_forward_button_pressed_callback (GtkWidget *widget, 
					 GdkEventButton *event, 
					 gpointer *user_data)
{
	NautilusWindow *window;
	gboolean back;

	g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (user_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	window = NAUTILUS_WINDOW (user_data);

	back = widget == get_back_button (window);
	g_assert (back || widget == get_forward_button (window));

	if (event->button == 3) {
		eel_pop_up_context_menu (
			create_back_or_forward_menu (NAUTILUS_WINDOW (user_data),
						     back),
                        EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
                        EEL_DEFAULT_POPUP_MENU_DISPLACEMENT,
                        event);

		return TRUE;
	}

	return FALSE;
	
}

static void
back_or_forward_button_clicked_callback (GtkWidget *widget, 
				   	 gpointer *user_data)
{
	NautilusWindow *window;
	gboolean back;

	g_return_if_fail (GTK_IS_BUTTON (widget));
	g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

	window = NAUTILUS_WINDOW (user_data);

	back = widget == get_back_button (window);
	g_assert (back || widget == get_forward_button (window));

	if (back) {
		nautilus_window_go_back (window);
	} else {
		nautilus_window_go_forward (window);
	}
}

static char *
get_file_name_from_icon_name (const char *icon_name, gboolean is_custom)
{
	char *full_path_name, *icon_theme, *theme_path_name;

	/* look in the theme to see if there's a redirection found; if so, prefer the
	 * redirection to the ordinary theme look-up */
	icon_theme = nautilus_theme_get_theme_data ("toolbar", "icon_theme");
	if (icon_theme != NULL) {
		/* special case the "standard" theme which indicates using the stock gnome icons,
		 * except for the custom ones, that are not present in stock
		 */
		if (!is_custom && eel_strcmp (icon_theme, "standard") == 0) {
			g_free (icon_theme);
			return NULL;
		}
		
		theme_path_name = g_strdup_printf ("%s/%s.png", icon_theme, icon_name);
		full_path_name = nautilus_pixmap_file (theme_path_name);
		if (full_path_name == NULL) {
			full_path_name = nautilus_theme_get_image_path (icon_name);
		}
		g_free (theme_path_name);
		g_free (icon_theme);
	} else {
		full_path_name = nautilus_theme_get_image_path (icon_name);
	}

	return full_path_name;
}

static void
set_up_standard_bonobo_button (NautilusWindow *window, 
			       const char *item_path, 
			       const char *icon_name,
			       const char *stock_item_fallback)
{
	char *file_name;

	file_name = get_file_name_from_icon_name (icon_name, (stock_item_fallback == NULL));
		
	/* set up the toolbar component with the new image */
	bonobo_ui_component_set_prop (window->details->shell_ui, 
				      item_path,
				      "pixtype",
				      file_name == NULL ? "stock" : "filename",
			      	      NULL);
	bonobo_ui_component_set_prop (window->details->shell_ui, 
				      item_path,
				      "pixname",
				      file_name == NULL ? stock_item_fallback : file_name,
			      	      NULL);

	g_free (file_name);
}

/* Use only for toolbar buttons that had to be explicitly created so they
 * could have behaviors not present in standard Bonobo toolbar buttons.
 */
static void
set_up_special_bonobo_button (NautilusWindow            *window,
			      BonoboUIToolbarButtonItem *item,
			      const char                *control_path,
			      const char                *icon_name,
			      const char                *stock_item_fallback)
{
	char *icon_file_name;
	GtkWidget *image;
	GtkStockItem stock_item;

	image = NULL;

	icon_file_name = get_file_name_from_icon_name (icon_name, FALSE);

	if (icon_file_name == NULL) {
		if (gtk_stock_lookup (stock_item_fallback, &stock_item)) {
			image = gtk_image_new_from_stock (stock_item_fallback, 
							  GTK_ICON_SIZE_BUTTON);
		}
	} else {
		image = gtk_image_new_from_file (icon_file_name);
		g_free (icon_file_name);
	}

	if (image == NULL) {
		return;
	}
	
	bonobo_ui_toolbar_button_item_set_image (item, image);
}			      

static void
set_up_toolbar_images (NautilusWindow *window)
{
	nautilus_window_ui_freeze (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	set_up_special_bonobo_button (window, window->details->back_button_item, 
				      "/Toolbar/BackWrapper", "Back", GTK_STOCK_GO_BACK);
	set_up_special_bonobo_button (window, window->details->forward_button_item, 
				      "/Toolbar/ForwardWrapper", "Forward", GTK_STOCK_GO_FORWARD);
	
	set_up_standard_bonobo_button (window, "/Toolbar/Up", "Up", GTK_STOCK_GO_UP);
	set_up_standard_bonobo_button (window, "/Toolbar/Home", "Home", GTK_STOCK_HOME);
	set_up_standard_bonobo_button (window, "/Toolbar/Reload", "Refresh", GTK_STOCK_REFRESH);
	set_up_standard_bonobo_button (window, "/Toolbar/Toggle Find Mode", "Search", GTK_STOCK_FIND);
	set_up_standard_bonobo_button (window, "/Toolbar/Stop", "Stop", GTK_STOCK_STOP);

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);

	nautilus_window_ui_thaw (window);
}


/* handle theme changes */
static void
theme_changed_callback (gpointer callback_data)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (callback_data);
	
	set_up_toolbar_images (window);
	
	/* if the toolbar is visible, toggle it's visibility to force a relayout */
	if (nautilus_window_toolbar_showing (window)) {
		nautilus_window_hide_toolbar (window);
		nautilus_window_show_toolbar (window);
	}
}

static void
back_or_forward_toolbar_item_property_set_cb (BonoboPropertyBag *bag,
					      const BonoboArg   *arg,
					      guint              arg_id,
					      CORBA_Environment *ev,
					      gpointer           user_data)
{
	BonoboControl *control;
	BonoboUIToolbarItem *item;
	GtkOrientation orientation;
	BonoboUIToolbarItemStyle style;

	control = BONOBO_CONTROL (user_data);
	item = BONOBO_UI_TOOLBAR_ITEM (bonobo_control_get_widget (control));

	switch (arg_id) {
	case TOOLBAR_ITEM_ORIENTATION_PROP:
		orientation = BONOBO_ARG_GET_INT (arg);
		bonobo_ui_toolbar_item_set_orientation (item, orientation);

		if (GTK_WIDGET (item)->parent) {
			gtk_widget_queue_resize (GTK_WIDGET (item)->parent);
		}
		break;
	case TOOLBAR_ITEM_STYLE_PROP:
		style = BONOBO_ARG_GET_INT (arg);
		bonobo_ui_toolbar_item_set_style (item, style);
		break;
	}
}

static BonoboUIToolbarButtonItem *
create_back_or_forward_toolbar_item (NautilusWindow *window, 
				     const char     *label, 
				     const char     *control_path)
{
	BonoboUIToolbarButtonItem *item;
	BonoboPropertyBag *pb;
	BonoboControl *wrapper;
	GtkButton *button;

	item = BONOBO_UI_TOOLBAR_BUTTON_ITEM 
		(bonobo_ui_toolbar_button_item_new (NULL, label)); /* Fill in image later */
	gtk_widget_show (GTK_WIDGET (item));

	button = bonobo_ui_toolbar_button_item_get_button_widget (item);
	g_signal_connect_object (button, "button_press_event",
				 G_CALLBACK (back_or_forward_button_pressed_callback),
				 window, 0);
	g_signal_connect_object (button, "clicked",
				 G_CALLBACK (back_or_forward_button_clicked_callback),
				 window, 0);

	wrapper = bonobo_control_new (GTK_WIDGET (item));
	pb = bonobo_property_bag_new
		(NULL, back_or_forward_toolbar_item_property_set_cb, wrapper);
	bonobo_property_bag_add (pb, "style",
				 TOOLBAR_ITEM_STYLE_PROP,
				 BONOBO_ARG_INT, NULL, NULL,
				 Bonobo_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (pb, "orientation",
				 TOOLBAR_ITEM_ORIENTATION_PROP,
				 BONOBO_ARG_INT, NULL, NULL,
				 Bonobo_PROPERTY_WRITEABLE);
	bonobo_control_set_properties (wrapper, BONOBO_OBJREF (pb), NULL);
	bonobo_object_unref (pb);

	bonobo_ui_component_object_set (window->details->shell_ui,
					control_path,
					BONOBO_OBJREF (wrapper),
					NULL);

	bonobo_object_unref (wrapper);

	return item;
}

static gboolean
location_change_at_idle_callback (gpointer callback_data)
{
	NautilusWindow *window;
	char *location;

	window = NAUTILUS_WINDOW (callback_data);

	location = window->details->location_to_change_to_at_idle;
	window->details->location_to_change_to_at_idle = NULL;
	window->details->location_change_at_idle_id = 0;

	nautilus_window_go_to (window, location);
	g_free (location);

	return FALSE;
}

/* handle bonobo events from the throbber -- since they can come in at
   any time right in the middle of things, defer until idle */
static void 
throbber_callback (BonoboListener *listener,
		   const char *event_name, 
		   const CORBA_any *arg,
		   CORBA_Environment *ev,
		   gpointer callback_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (callback_data);

	g_free (window->details->location_to_change_to_at_idle);
	window->details->location_to_change_to_at_idle = g_strdup (
		BONOBO_ARG_GET_STRING (arg));

	if (window->details->location_change_at_idle_id == 0) {
		window->details->location_change_at_idle_id =
			gtk_idle_add (location_change_at_idle_callback, window);
	}
}

static void
throbber_created_callback (Bonobo_Unknown     throbber,
			   CORBA_Environment *ev,
			   gpointer           user_data)
{
	char *exception_as_text;
	NautilusWindow *window;

	if (BONOBO_EX (ev)) {
		exception_as_text = bonobo_exception_get_text (ev);
		g_warning ("Throbber activation exception '%s'", exception_as_text);
		g_free (exception_as_text);
		return;
	}

	g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

	window = NAUTILUS_WINDOW (user_data);

	bonobo_ui_component_object_set (window->details->shell_ui,
					"/Toolbar/ThrobberWrapper",
					throbber, ev);
	CORBA_exception_free (ev);

	window->details->throbber_property_bag =
		Bonobo_Control_getProperties (throbber, ev);

	if (BONOBO_EX (ev)) {
		window->details->throbber_property_bag = CORBA_OBJECT_NIL;
		CORBA_exception_free (ev);
	} else {
		bonobo_pbclient_set_boolean (window->details->throbber_property_bag,
					     "throbbing",
					     window->details->throbber_active,
					     ev);
	}

	window->details->throbber_listener =
		bonobo_event_source_client_add_listener_full
		(window->details->throbber_property_bag,
		 g_cclosure_new (G_CALLBACK (throbber_callback), window, NULL), 
		 "Bonobo/Property:change:location", ev);

	bonobo_object_release_unref (throbber, ev);

	g_object_unref (window);
}

void
nautilus_window_allow_stop (NautilusWindow *window, gboolean allow)
{
	CORBA_Environment ev;
	
	if (( window->details->throbber_active &&  allow) ||
	    (!window->details->throbber_active && !allow)) {
		return;
	}

	if (allow)
		access ("nautilus-throbber: start", 0);
	else
		access ("nautilus-throbber: stop", 0);

	window->details->throbber_active = allow;

	nautilus_window_ui_freeze (window);

	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       NAUTILUS_COMMAND_STOP, allow);

	if (window->details->throbber_property_bag != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		bonobo_pbclient_set_boolean (window->details->throbber_property_bag,
					     "throbbing", allow, &ev);
		CORBA_exception_free (&ev);
	}

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_initialize_toolbars (NautilusWindow *window)
{
	CORBA_Environment ev;
	char *exception_as_text;

	nautilus_window_ui_freeze (window);

	/* FIXME bugzilla.gnome.org 41243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		CORBA_exception_init (&ev);

		g_object_ref (window);
		bonobo_get_object_async ("OAFIID:nautilus_throbber",
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
	}
	
	window->details->back_button_item = create_back_or_forward_toolbar_item 
		(window, _("Back"), "/Toolbar/BackWrapper");
	window->details->forward_button_item = create_back_or_forward_toolbar_item 
		(window, _("Forward"), "/Toolbar/ForwardWrapper");

	set_up_toolbar_images (window);

	eel_preferences_add_callback
		(NAUTILUS_PREFERENCES_THEME, 
		 theme_changed_callback,
		 window);

	nautilus_window_ui_thaw (window);
}
 
void
nautilus_window_toolbar_remove_theme_callback (NautilusWindow *window)
{
	eel_preferences_remove_callback
		(NAUTILUS_PREFERENCES_THEME,
		 theme_changed_callback,
		 window);
}
