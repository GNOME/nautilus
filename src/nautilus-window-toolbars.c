/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-window-toolbars.c - implementation of nautilus window toolbar operations,
                                split into separate file just for convenience.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-window-private.h"
#include "nautilus-application.h"

#include <gnome.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>

/* forward declarations */
static void toolbar_reload_callback (GtkWidget *widget, NautilusWindow *window);
static void toolbar_stop_callback (GtkWidget *widget, NautilusWindow *window);

/* toolbar definitions */

#define TOOLBAR_BACK_BUTTON_INDEX	0
#define TOOLBAR_FORWARD_BUTTON_INDEX	1
#define TOOLBAR_UP_BUTTON_INDEX		2
#define TOOLBAR_RELOAD_BUTTON_INDEX	3
/* separator */
#define TOOLBAR_HOME_BUTTON_INDEX	5
/* separator */
#define TOOLBAR_STOP_BUTTON_INDEX	7


static void
toolbar_back_callback (GtkWidget *widget, NautilusWindow *window)
{
  nautilus_window_go_back (window);
}

static void
toolbar_forward_callback (GtkWidget *widget, NautilusWindow *window)
{
  nautilus_window_go_forward (window);
}

static void
toolbar_up_callback (GtkWidget *widget, NautilusWindow *window)
{
  nautilus_window_go_up (window);
}

static void
toolbar_home_callback (GtkWidget *widget, NautilusWindow *window)
{
  nautilus_window_go_home (window);
}


static GnomeUIInfo toolbar_info[] = {
	GNOMEUIINFO_ITEM_STOCK
	(N_("Back"), N_("Go to the previously visited directory"),
	 toolbar_back_callback, "nautilus/eazel/Back.png"),
	GNOMEUIINFO_ITEM_STOCK
	(N_("Forward"), N_("Go to the next directory"),
	 toolbar_forward_callback, "nautilus/eazel/Forward.png"),
	GNOMEUIINFO_ITEM_STOCK
	(N_("Up"), N_("Go up a level in the directory heirarchy"),
	 toolbar_up_callback, "nautilus/eazel/Up.png"),
	GNOMEUIINFO_ITEM_STOCK
	(N_("Reload"), N_("Reload this view"),
	 toolbar_reload_callback, GNOME_STOCK_PIXMAP_REFRESH),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK
	(N_("Home"), N_("Go to your home directory"),
	 toolbar_home_callback, "nautilus/eazel/Home.png"),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_STOCK
	(N_("Stop"), N_("Interrupt loading"),
	 toolbar_stop_callback, GNOME_STOCK_PIXMAP_STOP),
	GNOMEUIINFO_END
};

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NautilusWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (NAUTILUS_IS_WINDOW (window));

	index = GPOINTER_TO_INT (gtk_object_get_user_data (GTK_OBJECT (menu_item)));
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
	GSList *list_link;
	int index;

	g_assert (NAUTILUS_IS_WINDOW (window));
	
	menu = GTK_MENU (gtk_menu_new ());

	list_link = back ? window->back_list : window->forward_list;
	index = 0;
	while (list_link != NULL)
	{
		menu_item = nautilus_bookmark_menu_item_new (NAUTILUS_BOOKMARK (list_link->data));		
		gtk_object_set_user_data (GTK_OBJECT (menu_item), GINT_TO_POINTER (index));
		gtk_widget_show (GTK_WIDGET (menu_item));
  		gtk_signal_connect(GTK_OBJECT(menu_item), 
  			"activate",
                     	back ? activate_back_menu_item_callback : activate_forward_menu_item_callback, 
                     	window);
		
		gtk_menu_append (menu, menu_item);
		list_link = g_slist_next (list_link);
		++index;
	}

	return menu;
}

static int
back_or_forward_button_clicked_callback (GtkWidget *widget, 
				   GdkEventButton *event, 
				   gpointer *user_data)
{
	gboolean back;

	g_return_val_if_fail (GTK_IS_BUTTON (widget), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (user_data), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	back = NAUTILUS_WINDOW (user_data)->back_button == widget;
	g_assert (back || NAUTILUS_WINDOW (user_data)->forward_button == widget);

	if (event->button == 3)
	{
		nautilus_pop_up_context_menu (
			create_back_or_forward_menu (NAUTILUS_WINDOW (user_data),
						     back),
                        NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
                        NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT);

		return TRUE;
	}

	return FALSE;
	
}

/* utility to remember newly allocated toolbar buttons for later enabling/disabling */
static void
remember_buttons(NautilusWindow *window, GnomeUIInfo current_toolbar_info[])
{
	window->back_button = current_toolbar_info[TOOLBAR_BACK_BUTTON_INDEX].widget;
	window->forward_button = current_toolbar_info[TOOLBAR_FORWARD_BUTTON_INDEX].widget;
	window->up_button = current_toolbar_info[TOOLBAR_UP_BUTTON_INDEX].widget;
	window->reload_button = current_toolbar_info[TOOLBAR_RELOAD_BUTTON_INDEX].widget;
	window->stop_button = current_toolbar_info[TOOLBAR_STOP_BUTTON_INDEX].widget;	
	window->home_button = current_toolbar_info[TOOLBAR_HOME_BUTTON_INDEX].widget;	
}


/* set up the toolbar info based on the current theme selection from preferences */

static void
setup_button(GtkWidget* button,  const char *icon_name)
{
	GList *list;
	GtkWidget *widget;
	
	list = gtk_container_children(GTK_CONTAINER(GTK_BIN(button)->child));
	widget = GTK_WIDGET(list->data);
	g_list_free(list);
			
	gnome_stock_set_icon(GNOME_STOCK(widget), icon_name);
	gtk_widget_queue_resize(button); 
}


static void
setup_toolbar_images(NautilusWindow *window)
{
	gboolean use_eazel_theme;
	
	use_eazel_theme = nautilus_preferences_get_boolean(NAUTILUS_PREFERENCES_EAZEL_TOOLBAR_ICONS, FALSE);
	
	setup_button (window->back_button, use_eazel_theme ?  "nautilus/eazel/Back.png" : GNOME_STOCK_PIXMAP_BACK);
	setup_button (window->forward_button, use_eazel_theme ? "nautilus/eazel/Forward.png" : GNOME_STOCK_PIXMAP_FORWARD);
	setup_button (window->up_button, use_eazel_theme ? "nautilus/eazel/Up.png" : GNOME_STOCK_PIXMAP_UP);
	setup_button (window->home_button, use_eazel_theme ? "nautilus/eazel/Home.png" : GNOME_STOCK_PIXMAP_HOME);
}

/* allocate a new toolbar */
void
nautilus_window_initialize_toolbars (NautilusWindow *window)
{
	GnomeApp *app;
	GtkWidget *toolbar;
	
	app = GNOME_APP (window);	
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_BOTH);	
        gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar), toolbar_info, app->accel_group, app);	
	remember_buttons(window, toolbar_info);
	setup_toolbar_images(window);

	gnome_app_set_toolbar (app, GTK_TOOLBAR (toolbar));
			
	bonobo_ui_handler_set_toolbar (window->uih, "Main", toolbar);

	gtk_signal_connect (GTK_OBJECT (window->back_button),
	      "button_press_event",
	      GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback), 
	      window);

	gtk_signal_connect (GTK_OBJECT (window->forward_button),
	      "button_press_event",
	      GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback), 
	      window);

	/* add callback for preference changes */
	nautilus_preferences_add_callback(NAUTILUS_PREFERENCES_EAZEL_TOOLBAR_ICONS, 
						(NautilusPreferencesCallback) setup_toolbar_images, 
						window);
}
 
void
nautilus_window_toolbar_remove_theme_callback()
{
nautilus_preferences_remove_callback(NAUTILUS_PREFERENCES_EAZEL_TOOLBAR_ICONS,
				     (NautilusPreferencesCallback) setup_toolbar_images, NULL);
}
 
 
static void
toolbar_reload_callback (GtkWidget *widget, NautilusWindow *window)
{
	Nautilus_NavigationRequestInfo nri;

	memset(&nri, 0, sizeof(nri));
	nri.requested_uri = (char *)nautilus_window_get_requested_uri (window);
	nri.new_window_requested = FALSE;
	nautilus_window_begin_location_change (window, &nri, NULL, NAUTILUS_LOCATION_CHANGE_RELOAD, 0);
}

static void
toolbar_stop_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_set_state_info (window, RESET_TO_IDLE, 0);
}

