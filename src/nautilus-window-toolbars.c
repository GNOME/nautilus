/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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

#include "nautilus-application.h"
#include "nautilus-throbber.h"
#include "nautilus-toolbar.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <gnome.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-theme.h>

/* forward declarations */
static void toolbar_reload_callback (GtkWidget *widget, NautilusWindow *window);
static void toolbar_stop_callback (GtkWidget *widget, NautilusWindow *window);
#ifdef EAZEL_SERVICES
static void toolbar_services_callback (GtkWidget *widget, NautilusWindow *window);
#endif

/* toolbar definitions */

#define TOOLBAR_BACK_BUTTON_INDEX		0
#define TOOLBAR_FORWARD_BUTTON_INDEX		1
#define TOOLBAR_UP_BUTTON_INDEX			2
#define TOOLBAR_RELOAD_BUTTON_INDEX		3
/* separator */
#define TOOLBAR_HOME_BUTTON_INDEX		5
#define TOOLBAR_SEARCH_LOCAL_BUTTON_INDEX	6
#define TOOLBAR_SEARCH_WEB_BUTTON_INDEX		7
/* separator */
#define TOOLBAR_STOP_BUTTON_INDEX		9
#define TOOLBAR_SERVICES_INDEX			10

#define GNOME_STOCK_PIXMAP_WEBSEARCH "SearchWeb"

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

static void
toolbar_search_local_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_set_search_mode (window, GTK_TOGGLE_BUTTON (widget)->active);
}

static void
toolbar_search_web_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_go_web_search (window);
}

#define NAUTILUS_GNOMEUIINFO_TOGGLEITEM_STOCK(label, tooltip, callback, stock_id) \
	{ GNOME_APP_UI_TOGGLEITEM, label, tooltip, (gpointer)callback, NULL, NULL, \
		GNOME_APP_PIXMAP_STOCK, stock_id, 0, (GdkModifierType) 0, NULL }

static GnomeUIInfo toolbar_info[] = {
	GNOMEUIINFO_ITEM_STOCK
	(N_("Back"), NAUTILUS_HINT_BACK,
	 toolbar_back_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Back.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Forward"), NAUTILUS_HINT_FORWARD,
	 toolbar_forward_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Forward.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Up"), NAUTILUS_HINT_UP,
	 toolbar_up_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Up.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Refresh"), NAUTILUS_HINT_REFRESH,
	 toolbar_reload_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Refresh.png"),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Home"), NAUTILUS_HINT_HOME,
	 toolbar_home_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Home.png"),
	
	NAUTILUS_GNOMEUIINFO_TOGGLEITEM_STOCK
	(N_("Find"), NAUTILUS_HINT_FIND,
	 toolbar_search_local_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Search.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Web Search"), NAUTILUS_HINT_WEB_SEARCH,
	 toolbar_search_web_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/SearchWeb.png"),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Stop"), NAUTILUS_HINT_STOP,
	 toolbar_stop_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Stop.png"),

#ifdef EAZEL_SERVICES
	GNOMEUIINFO_ITEM_STOCK
	(N_("Services"), NAUTILUS_HINT_SERVICES,
	 toolbar_services_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Services.png"),
#endif

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
	GList *list_link;
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
  		gtk_signal_connect
			(GTK_OBJECT(menu_item), 
			 "activate",
			 back ? activate_back_menu_item_callback : activate_forward_menu_item_callback, 
			 window);
		
		gtk_menu_append (menu, menu_item);
		list_link = g_list_next (list_link);
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

	if (event->button == 3) {
		nautilus_pop_up_context_menu (
			create_back_or_forward_menu (NAUTILUS_WINDOW (user_data),
						     back),
                        NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
                        NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
                        event->button);

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
	window->search_local_button = current_toolbar_info[TOOLBAR_SEARCH_LOCAL_BUTTON_INDEX].widget;
	window->search_web_button = current_toolbar_info[TOOLBAR_SEARCH_WEB_BUTTON_INDEX].widget;
	window->stop_button = current_toolbar_info[TOOLBAR_STOP_BUTTON_INDEX].widget;	
	window->home_button = current_toolbar_info[TOOLBAR_HOME_BUTTON_INDEX].widget;	
}

/* find the toolbar child structure within a toolbar.  This way we can easily
 * find the icon in a clean way */
static GtkToolbarChild *
find_toolbar_child(GtkToolbar *toolbar, GtkWidget *button)
{
	GList *li;
	for (li = toolbar->children; li != NULL; li = li->next) {
		GtkToolbarChild *child = li->data;
		if (child->widget == button)
			return child;
	}
	return NULL;
}

/* set up the toolbar info based on the current theme selection from preferences */

static void
set_up_button (GtkWidget* button,
	       const char *icon_name)
{
	GnomeStock *stock_widget;
	char *full_name;
	GtkToolbarChild *toolbar_child;

	full_name = nautilus_theme_get_image_path (icon_name);
	if (full_name == NULL) {
		full_name = g_strdup (icon_name);	
	}
		
	toolbar_child = find_toolbar_child (GTK_TOOLBAR (button->parent), button);
	if (toolbar_child != NULL
	    && toolbar_child->icon != NULL
	    && GNOME_IS_STOCK (toolbar_child->icon)) {
		stock_widget = GNOME_STOCK (toolbar_child->icon);
	} else {
		stock_widget = NULL;
	}
			
	if (stock_widget != NULL) {
		/* We can't just gnome_stock_set_icon here, as that
		 * doesn't register new pixmaps automatically */
		nautilus_gnome_stock_set_icon_or_register (stock_widget,
							   full_name);
	}

	g_free (full_name);

	gtk_widget_queue_resize (button);
}


static void
set_up_toolbar_images (NautilusWindow *window)
{
	set_up_button (window->back_button, GNOME_STOCK_PIXMAP_BACK);
	set_up_button (window->forward_button, GNOME_STOCK_PIXMAP_FORWARD);
	set_up_button (window->up_button, GNOME_STOCK_PIXMAP_UP);
	set_up_button (window->home_button, GNOME_STOCK_PIXMAP_HOME);
	set_up_button (window->reload_button, GNOME_STOCK_PIXMAP_REFRESH);
	set_up_button (window->search_local_button, GNOME_STOCK_PIXMAP_SEARCH);
	set_up_button (window->search_web_button, GNOME_STOCK_PIXMAP_WEBSEARCH);
	set_up_button (window->stop_button, GNOME_STOCK_PIXMAP_STOP);
}

static GtkWidget*
allocate_throbber (GtkWidget *toolbar)
{
	GtkWidget *throbber;
	
	throbber = nautilus_throbber_new ();
	gtk_widget_show (throbber);
	gtk_toolbar_append_widget (GTK_TOOLBAR (toolbar), throbber, NULL, NULL);
	nautilus_toolbar_set_throbber (NAUTILUS_TOOLBAR (toolbar), throbber);
	return throbber;
}

static void
set_up_toolbar_images_callback (gpointer callback_data)
{
	set_up_toolbar_images (NAUTILUS_WINDOW (callback_data));
}

/* allocate a new toolbar */
void
nautilus_window_initialize_toolbars (NautilusWindow *window)
{
	GnomeApp *app;
	GtkWidget *toolbar;
	
	app = GNOME_APP (window);	
	
	toolbar = nautilus_toolbar_new ();	
        gtk_toolbar_set_orientation (GTK_TOOLBAR (toolbar), GTK_ORIENTATION_HORIZONTAL); 
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH); 
	nautilus_toolbar_set_button_spacing (NAUTILUS_TOOLBAR (toolbar), 50);
	
	gnome_app_fill_toolbar_with_data (GTK_TOOLBAR (toolbar), toolbar_info, app->accel_group, app);
	remember_buttons(window, toolbar_info);
	window->throbber = allocate_throbber (toolbar);
	
	set_up_toolbar_images (window);

	gnome_app_set_toolbar (app, GTK_TOOLBAR (toolbar));
			
	bonobo_ui_handler_set_toolbar (window->ui_handler, "Main", toolbar);

	gtk_signal_connect (GTK_OBJECT (window->back_button),
			    "button_press_event",
			    GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback), 
			    window);

	gtk_signal_connect (GTK_OBJECT (window->forward_button),
			    "button_press_event",
			    GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback), 
			    window);
	
	/* add callback for preference changes */
	nautilus_preferences_add_callback
		(NAUTILUS_PREFERENCES_THEME, 
		 set_up_toolbar_images_callback,
		 window);
}
 
void
nautilus_window_toolbar_remove_theme_callback (NautilusWindow *window)
{
	nautilus_preferences_remove_callback
		(NAUTILUS_PREFERENCES_THEME,
		 set_up_toolbar_images_callback,
		 window);
}
 
 
static void
toolbar_reload_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_reload (window);
}

static void
toolbar_stop_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_set_state_info (window, RESET_TO_IDLE, 0);
}

#ifdef EAZEL_SERVICES
static void
toolbar_services_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_goto_uri (window, "eazel:");
}
#endif
