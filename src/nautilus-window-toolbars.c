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
#include "nautilus-toolbar.h"
#include "nautilus-window.h"
#include "nautilus-window-private.h"
#include "nautilus-application.h"

#include <gnome.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-theme.h>

/* forward declarations */
static void toolbar_reload_callback (GtkWidget *widget, NautilusWindow *window);
static void toolbar_stop_callback (GtkWidget *widget, NautilusWindow *window);
#if defined(EAZEL_SERVICES)
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

static void
toolbar_clear_search_mode(NautilusWindow *window)
{
  GtkToggleButton *button;
  
  button = GTK_TOGGLE_BUTTON(window->search_local_button);
  if (button->active) {
  	nautilus_window_set_search_mode (window, FALSE);
  	gtk_toggle_button_set_active(button, FALSE);
  }
}

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
  toolbar_clear_search_mode (window);  
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
  	toolbar_clear_search_mode (window);  
	nautilus_window_go_web_search (window);
}

#define NAUTILUS_GNOMEUIINFO_TOGGLEITEM_STOCK(label, tooltip, callback, stock_id) \
	{ GNOME_APP_UI_TOGGLEITEM, label, tooltip, (gpointer)callback, NULL, NULL, \
		GNOME_APP_PIXMAP_STOCK, stock_id, 0, (GdkModifierType) 0, NULL }


static GnomeUIInfo toolbar_info[] = {
	GNOMEUIINFO_ITEM_STOCK
	(N_("Back"), N_("Go to the previously visited directory"),
	 toolbar_back_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Back.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Forward"), N_("Go to the next directory"),
	 toolbar_forward_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Forward.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Up"), N_("Go up a level in the directory hierarchy"),
	 toolbar_up_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Up.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Reload"), N_("Reload this view"),
	 toolbar_reload_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Refresh.png"),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Home"), N_("Go to your home directory"),
	 toolbar_home_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Home.png"),
	
	NAUTILUS_GNOMEUIINFO_TOGGLEITEM_STOCK
	(N_("Search"), N_("Search this computer for files"),
	 toolbar_search_local_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Search.png"),
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Web Search"), N_("Search the web"),
	 toolbar_search_web_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Search.png"),
	
	GNOMEUIINFO_SEPARATOR,
	
	GNOMEUIINFO_ITEM_STOCK
	(N_("Stop"), N_("Interrupt loading"),
	 toolbar_stop_callback,
	 NAUTILUS_PIXMAPDIR "/eazel/Stop.png"),
#if defined(EAZEL_SERVICES)
	GNOMEUIINFO_ITEM_STOCK
	(N_("Services"), N_("Eazel Services"),
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
	       const char *theme_name,
	       const char *icon_name)
{
	GnomeStock *stock_widget;
	char *full_name;
	GtkToolbarChild *toolbar_child;

	if ((theme_name == NULL) || (strcmp(theme_name, "default") == 0)) {
		full_name = g_strdup (icon_name);
	} else {
		full_name = g_strdup_printf (NAUTILUS_PIXMAPDIR "/%s/%s.png", theme_name, icon_name);
	}
	
	toolbar_child = find_toolbar_child (GTK_TOOLBAR (button->parent), button);
	if (toolbar_child != NULL &&
	    toolbar_child->icon != NULL &&
	    GNOME_IS_STOCK (toolbar_child->icon))
		stock_widget = GNOME_STOCK (toolbar_child->icon);
	else
		stock_widget = NULL;
			
	if (stock_widget != NULL &&
	    ! gnome_stock_set_icon (stock_widget, full_name) &&
	    g_file_exists(full_name)) {
		/* if full_name exists but gnome_stock_set_icon fails, that means
		 * this file has NOT been registered with gnome stock.  Unfortunately
		 * gnome_stock is a worthless pile of dung and doesn't do this for us.
		 * Do note however that it DOES register this stuff when it first
		 * creates the toolbars from GnomeUIInfo. */
		GnomeStockPixmapEntryPath *new_entry;
		new_entry = g_malloc(sizeof(GnomeStockPixmapEntryPath));
		new_entry->type = GNOME_STOCK_PIXMAP_TYPE_PATH;
		new_entry->label = NULL;
		new_entry->pathname = full_name;
		new_entry->width = 0;
		new_entry->height = 0;
		/* register this under the "full_name" as that's what we'll look it
		 * up under later */
		gnome_stock_pixmap_register(full_name, GNOME_STOCK_PIXMAP_REGULAR,
					    (GnomeStockPixmapEntry *)new_entry);
		full_name = NULL; /* we used it in new_entry, so we just transfer
				     ownership */
		gnome_stock_set_icon (stock_widget, full_name);
	}

	g_free (full_name);
	gtk_widget_queue_resize (button);
}


static void
set_up_toolbar_images (NautilusWindow *window)
{
	char *theme_name;
	
	theme_name = nautilus_theme_get_theme_data ("toolbar", "ICON_THEME");

	set_up_button (window->back_button, theme_name, GNOME_STOCK_PIXMAP_BACK);
	set_up_button (window->forward_button, theme_name, GNOME_STOCK_PIXMAP_FORWARD);
	set_up_button (window->up_button, theme_name, GNOME_STOCK_PIXMAP_UP);
	set_up_button (window->home_button, theme_name,  GNOME_STOCK_PIXMAP_HOME);
	set_up_button (window->reload_button, theme_name,  GNOME_STOCK_PIXMAP_REFRESH);
	set_up_button (window->search_local_button, theme_name, GNOME_STOCK_PIXMAP_SEARCH);
	set_up_button (window->search_web_button, theme_name, GNOME_STOCK_PIXMAP_SEARCH);
	set_up_button (window->stop_button, theme_name, GNOME_STOCK_PIXMAP_STOP);

	g_free(theme_name);
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

#if defined(EAZEL_SERVICES)
static void
toolbar_services_callback (GtkWidget *widget, NautilusWindow *window)
{
	nautilus_window_goto_uri (window, "http://www.eazel.com/services.html");
}
#endif

