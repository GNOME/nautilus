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
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <bonobo/bonobo-control.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktogglebutton.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-bookmark.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-theme.h>

#ifdef UIH

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

#endif /* UIH */

/* set up the toolbar info based on the current theme selection from preferences */

static void
set_up_button (NautilusWindow *window, const char *item_path, const char *icon_name)
{
	char *full_name, *icon_theme, *path_name;

	/* look in the theme to see if there's a redirection found */
	icon_theme = nautilus_theme_get_theme_data ("toolbar", "ICON_THEME");
	if (icon_theme != NULL) {
		path_name = g_strdup_printf ("%s/%s.png", icon_theme, icon_name);
		full_name = nautilus_pixmap_file (path_name);
		g_free (path_name);
		g_free (icon_theme);
	} else {
		full_name = nautilus_theme_get_image_path (icon_name);
	}
		
	/* set up the toolbar component with the new image */
	bonobo_ui_component_freeze (window->details->shell_ui, NULL);
	bonobo_ui_component_set_prop (window->details->shell_ui, 
				      item_path,
				      "pixtype",
				      full_name == NULL ? "stock" : "filename",
			      	      NULL);
	bonobo_ui_component_set_prop (window->details->shell_ui, 
				      item_path,
				      "pixname",
				      full_name == NULL ? icon_name : full_name,
			      	      NULL);
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);

	g_free (full_name);
}


static void
set_up_toolbar_images (NautilusWindow *window)
{
	set_up_button (window, "/Tool Bar/Back", "Back");
	set_up_button (window, "/Tool Bar/Forward", "Forward");
	set_up_button (window, "/Tool Bar/Up", "Up");
	set_up_button (window, "/Tool Bar/Home", "Home");
	set_up_button (window, "/Tool Bar/Reload", "Refresh");
	set_up_button (window, "/Tool Bar/Toggle Find Mode", "Search");
	set_up_button (window, "/Tool Bar/Go to Web Search", "SearchWeb");
	set_up_button (window, "/Tool Bar/Stop", "Stop");
#ifdef EAZEL_SERVICES	
	set_up_button (window, "/Tool Bar/Extra Buttons Placeholder/Services", "Services");
#endif
}

static GtkWidget *
set_up_throbber_frame_type (NautilusWindow *window)
{
	GtkWidget *frame;
	char *frame_type;
	GtkShadowType shadow_type;
	
	frame = window->throbber->parent;
	
	frame_type = nautilus_theme_get_theme_data ("toolbar", "FRAME_TYPE");
	shadow_type = GTK_SHADOW_NONE;
	
	if (nautilus_strcmp (frame_type, "in") == 0) {
		shadow_type = GTK_SHADOW_IN;
	} else if (nautilus_strcmp (frame_type, "out") == 0) {
		shadow_type = GTK_SHADOW_OUT;	
	} else if (nautilus_strcmp (frame_type, "none") == 0) {
		shadow_type = GTK_SHADOW_NONE;	
	} else {
		shadow_type = GTK_SHADOW_NONE;
	}
	
	g_free (frame_type);	
	
	/* FIXME: for now, force no shadow until bonobo toolbar problems get resolved */
	shadow_type = GTK_SHADOW_NONE;
	gtk_frame_set_shadow_type (GTK_FRAME (frame), shadow_type);
	
	return frame;
}

static GtkWidget *
allocate_throbber (void)
{
	GtkWidget *throbber, *frame;
	gboolean small_mode;
	
	throbber = nautilus_throbber_new ();
	gtk_widget_show (throbber);
	
	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (frame), throbber);
			
	small_mode = FALSE; /* for now - want to query the toolbar's style soon */
	nautilus_throbber_set_small_mode (NAUTILUS_THROBBER (throbber), small_mode);
	
	return throbber;
}

/* handle theme changes */
static void
theme_changed_callback (gpointer callback_data)
{
	set_up_toolbar_images (NAUTILUS_WINDOW (callback_data));
	set_up_throbber_frame_type (NAUTILUS_WINDOW (callback_data));
}

/* initialize the toolbar */
void
nautilus_window_initialize_toolbars (NautilusWindow *window)
{
	GtkWidget *frame, *box;
	BonoboControl *throbber_wrapper;
	
	set_up_toolbar_images (window);

	window->throbber = allocate_throbber ();	
	frame = set_up_throbber_frame_type (window);
	
	/* wrap it in another box to add some visual padding */
	box = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (box), 4);
	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (box), frame);
	
	throbber_wrapper = bonobo_control_new (box);
	
	bonobo_ui_component_object_set (window->details->shell_ui,
					"/Tool Bar/ThrobberWrapper",
					bonobo_object_corba_objref (BONOBO_OBJECT (throbber_wrapper)),
					NULL);
	
	bonobo_object_unref (BONOBO_OBJECT (throbber_wrapper));

#ifdef UIH
	gtk_signal_connect (GTK_OBJECT (window->back_button),
			    "button_press_event",
			    GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback), 
			    window);

	gtk_signal_connect (GTK_OBJECT (window->forward_button),
			    "button_press_event",
			    GTK_SIGNAL_FUNC (back_or_forward_button_clicked_callback), 
			    window);
#endif
	
	/* add callback for preference changes */
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
