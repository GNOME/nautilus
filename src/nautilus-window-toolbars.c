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
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <bonobo.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktogglebutton.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-preferences.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-theme.h>

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
		nautilus_pop_up_context_menu (
			create_back_or_forward_menu (NAUTILUS_WINDOW (user_data),
						     back),
                        NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
                        NAUTILUS_DEFAULT_POPUP_MENU_DISPLACEMENT,
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
get_file_name_from_icon_name (const char *icon_name)
{
	char *full_path_name, *icon_theme, *theme_path_name;

	/* look in the theme to see if there's a redirection found */
	icon_theme = nautilus_theme_get_theme_data ("toolbar", "icon_theme");
	if (icon_theme != NULL) {
		/* special case the "standard" theme which indicates using the stock gnome icons */
		if (nautilus_strcmp (icon_theme, "standard") == 0) {
			g_free (icon_theme);
			return NULL;
		}
		
		theme_path_name = g_strdup_printf ("%s/%s.png", icon_theme, icon_name);
		full_path_name = nautilus_pixmap_file (theme_path_name);
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
			       const char *icon_name)
{
	char *file_name;

	file_name = get_file_name_from_icon_name (icon_name);
		
	/* set up the toolbar component with the new image */
	bonobo_ui_component_set_prop (window->details->shell_ui, 
				      item_path,
				      "pixtype",
				      file_name == NULL ? "stock" : "filename",
			      	      NULL);
	bonobo_ui_component_set_prop (window->details->shell_ui, 
				      item_path,
				      "pixname",
				      file_name == NULL ? icon_name : file_name,
			      	      NULL);

	g_free (file_name);
}

static GdkPixbuf *
get_pixbuf_for_xml_node (NautilusWindow *window, const char *node_path)
{
	BonoboUINode *node;
	GdkPixbuf *pixbuf;

	node = bonobo_ui_component_get_tree (window->details->shell_ui, node_path, FALSE, NULL);
	pixbuf = bonobo_ui_util_xml_get_icon_pixbuf (node, FALSE);
	bonobo_ui_node_free (node);

	return pixbuf;
}

/* Use only for tool bar buttons that had to be explicitly created so they
 * could have behaviors not present in standard Bonobo tool bar buttons.
 */
static void
set_up_special_bonobo_button (NautilusWindow *window,
			      BonoboUIToolbarButtonItem *item,
			      const char *control_path,
			      const char *icon_name)
{
	char *icon_file_name;
	GdkPixbuf *pixbuf;	

	icon_file_name = get_file_name_from_icon_name (icon_name);

	if (icon_file_name == NULL) {
		pixbuf = get_pixbuf_for_xml_node (window, control_path);
	} else {
		pixbuf = gdk_pixbuf_new_from_file (icon_file_name);
		g_free (icon_file_name);
	}
	
	bonobo_ui_toolbar_button_item_set_icon (item, pixbuf);
	gdk_pixbuf_unref (pixbuf);

	/* FIXME bugzilla.eazel.com 5005:
	 * Setting the style here accounts for the preference, but does not
	 * account for a hard-wired tool bar style or later changes in style
	 * (such as if the tool bar is detached and made vertical). There
	 * is currently no Bonobo API to support matching the style properly.
	 */
	bonobo_ui_toolbar_item_set_style 
		(BONOBO_UI_TOOLBAR_ITEM (item),
		 gnome_preferences_get_toolbar_labels ()
		 	? BONOBO_UI_TOOLBAR_ITEM_STYLE_ICON_AND_TEXT_VERTICAL
		 	: BONOBO_UI_TOOLBAR_ITEM_STYLE_ICON_ONLY);
}			      


static void
set_up_toolbar_images (NautilusWindow *window)
{
	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	set_up_special_bonobo_button (window, window->details->back_button_item, "/Tool Bar/BackWrapper", "Back");
	set_up_special_bonobo_button (window, window->details->forward_button_item, "/Tool Bar/ForwardWrapper", "Forward");
	
	set_up_standard_bonobo_button (window, "/Tool Bar/Up", "Up");
	set_up_standard_bonobo_button (window, "/Tool Bar/Home", "Home");
	set_up_standard_bonobo_button (window, "/Tool Bar/Reload", "Refresh");
	set_up_standard_bonobo_button (window, "/Tool Bar/Toggle Find Mode", "Search");
	set_up_standard_bonobo_button (window, "/Tool Bar/Go to Web Search", "SearchWeb");
	set_up_standard_bonobo_button (window, "/Tool Bar/Stop", "Stop");
#ifdef EAZEL_SERVICES	
	set_up_standard_bonobo_button (window, "/Tool Bar/Extra Buttons Placeholder/Services", "Services");
#endif
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
}


/* handle theme changes */
static void
theme_changed_callback (gpointer callback_data)
{
	NautilusWindow *window;
	
	window = NAUTILUS_WINDOW (callback_data);
	
	set_up_toolbar_images (window);
	
	/* if the toolbar is visible, toggle it's visibility to force a relayout */
	if (nautilus_window_tool_bar_showing (window)) {
		nautilus_window_hide_tool_bar (window);
		nautilus_window_show_tool_bar (window);
	}
}

static void
set_widget_for_bonobo_control (NautilusWindow *window,
			       GtkWidget *widget,
			       const char *control_path)
{
	BonoboControl *wrapper;

	wrapper = bonobo_control_new (widget);
	bonobo_ui_component_object_set (window->details->shell_ui,
					control_path,
					bonobo_object_corba_objref (BONOBO_OBJECT (wrapper)),
					NULL);
	bonobo_object_unref (BONOBO_OBJECT (wrapper));
}

static BonoboUIToolbarButtonItem *
set_up_back_or_forward_tool_bar_item (NautilusWindow *window, 
				      const char *label, 
				      const char *control_path)
{
	BonoboUIToolbarButtonItem *item;
	GtkButton *button;

	item = BONOBO_UI_TOOLBAR_BUTTON_ITEM 
		(bonobo_ui_toolbar_button_item_new (NULL, label)); /* Fill in image later */
	gtk_widget_show (GTK_WIDGET (item));

	button = bonobo_ui_toolbar_button_item_get_button_widget (item);
	gtk_signal_connect (GTK_OBJECT (button),
			    "button_press_event",
			    GTK_SIGNAL_FUNC (back_or_forward_button_pressed_callback),
			    window);
	gtk_signal_connect (GTK_OBJECT (button),
			    "clicked",
			    GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback),
			    window);
	set_widget_for_bonobo_control (window, GTK_WIDGET (item), control_path);

	return item;
}
			       

void
nautilus_window_initialize_toolbars (NautilusWindow *window)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	window->throbber = bonobo_get_object ("OAFIID:nautilus_throbber", "IDL:Bonobo/Control:1.0", &ev);
	
	
	if (BONOBO_EX (&ev)) {
		char *txt;
		g_warning ("Throbber Activation exception '%s'",
			   (txt = bonobo_exception_get_text (&ev)));
		g_free (txt);
		window->throbber = CORBA_OBJECT_NIL;
	}

	bonobo_ui_component_object_set (window->details->shell_ui,
					"/Tool Bar/ThrobberWrapper",
					window->throbber,
					&ev);
	CORBA_exception_free (&ev);
	
	window->details->back_button_item = set_up_back_or_forward_tool_bar_item 
		(window, _("Back"), "/Tool Bar/BackWrapper");
	window->details->forward_button_item = set_up_back_or_forward_tool_bar_item 
		(window, _("Forward"), "/Tool Bar/ForwardWrapper");

	set_up_toolbar_images (window);

	nautilus_preferences_add_callback
		(NAUTILUS_PREFERENCES_THEME, 
		 theme_changed_callback,
		 window);
}
 
void
nautilus_window_toolbar_remove_theme_callback (NautilusWindow *window)
{
	nautilus_preferences_remove_callback
		(NAUTILUS_PREFERENCES_THEME,
		 theme_changed_callback,
		 window);
}
