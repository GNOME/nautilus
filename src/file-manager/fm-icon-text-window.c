/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-text-window.c - interface for window that lets user modify 
 			   displayed icon text.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "fm-icon-text-window.h"

#include <gtk/gtkaccellabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>

static void ensure_unique_attributes (int menu_index);
static gboolean fm_icon_text_window_delete_event_cb (GtkWidget *widget,
				  		     GdkEvent  *event,
				  		     gpointer   user_data);


#define PIECES_COUNT	4
#define MENU_COUNT	(PIECES_COUNT - 1)

static GtkOptionMenu *option_menus[MENU_COUNT];

static char * attribute_names[] = {
	"size",
	"type",
	"date_modified",
	"date_changed",
	"date_accessed",
	"owner",
	"group",
	"permissions",
	NULL
};

static char * attribute_labels[] = {
	_("size"),
	_("type"),
	_("date modified"),
	_("date changed"),
	_("date accessed"),
	_("owner"),
	_("group"),
	_("permissions"),
	NULL
};

static int
get_attribute_index_from_option_menu (GtkOptionMenu *option_menu)
{
	g_assert (GTK_IS_OPTION_MENU (option_menu));
	return GPOINTER_TO_INT (gtk_object_get_user_data (
		GTK_OBJECT (option_menu->menu_item)));
}

static char *
get_chosen_attribute_name (GtkOptionMenu *option_menu)
{
	int attribute_index;
	
	g_assert (GTK_IS_OPTION_MENU (option_menu));

	attribute_index = get_attribute_index_from_option_menu (option_menu);

	return g_strdup (attribute_names[attribute_index]);
}

static void
changed_attributes_option_menu_cb (GtkMenuItem *menu_item, gpointer user_data)
{
	char ** attribute_names_array;
	char * attribute_names_string;
	int which_menu;
	int index;
	
	which_menu = GPOINTER_TO_INT (user_data);
  	attribute_names_array = g_new0 (gchar*, PIECES_COUNT + 1);

	/* Check whether just-changed item matches any others. If so,
	 * change the other one to the first unused attribute.
	 */
	ensure_unique_attributes (which_menu);
	
	/* Always start with the name. */
	attribute_names_array[0] = g_strdup ("name");

	for (index = 0; index < MENU_COUNT; ++index)
	{
		attribute_names_array[index+1] = 
			get_chosen_attribute_name (option_menus[index]);
	}

	attribute_names_string = g_strjoinv ("|", attribute_names_array);

	fm_directory_view_icons_set_full_icon_text_attribute_names (NULL, attribute_names_string);

	g_free (attribute_names_string);
	g_strfreev (attribute_names_array);
}

static GtkOptionMenu *
create_attributes_option_menu (int menu_number)
{
	GtkWidget *option_menu;
	GtkWidget *menu;
	int index;

  	option_menu = gtk_option_menu_new ();
  	gtk_widget_show (option_menu);
  	menu = gtk_menu_new ();
	
	for (index = 0; attribute_names[index] != NULL; ++index)
	{
		GtkWidget *menu_item;
		GtkWidget *accel_label;
	
		menu_item = gtk_menu_item_new ();

		/* Do some extra label-creating work so they're centered */
		accel_label = gtk_accel_label_new (attribute_labels[index]);
		gtk_misc_set_alignment (GTK_MISC (accel_label), 0.5, 0.5);
		gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
		gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), menu_item);
		gtk_widget_show (accel_label);

		
		/* Store index in item as the way to get from item back to attribute name */
		gtk_object_set_user_data (GTK_OBJECT (menu_item), GINT_TO_POINTER (index));
		gtk_widget_show (menu_item);
		gtk_menu_append (GTK_MENU (menu), menu_item);
		/* Wire all the menu items to the same callback. If any item is
		 * changed, the attribute text will be recomputed from scratch.
		 */
	  	gtk_signal_connect (GTK_OBJECT (menu_item),
	  			      "activate",
	  			      changed_attributes_option_menu_cb,
	  			      GINT_TO_POINTER (menu_number));
		
	}
  	  	
  	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	return GTK_OPTION_MENU (option_menu);
}

static GtkWidget*
create_icon_text_window ()
{
  	GtkWidget *window;
  	GtkWidget *contents_vbox;
  	GtkWidget *prompt;
  	GtkWidget *separator_line;
  	GtkWidget *hbox_to_center_menus;
  	GtkWidget *menus_vbox;
  	int index;

  	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus: Icon Text"));
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);

  	contents_vbox = gtk_vbox_new (FALSE, 0);
  	gtk_widget_show (contents_vbox);
  	gtk_container_add (GTK_CONTAINER (window), contents_vbox);

  	prompt = gtk_label_new (_("Choose the order for information to appear beneath icon names. More information appears as you zoom in closer."));
  	gtk_widget_show (prompt);
  	gtk_box_pack_start (GTK_BOX (contents_vbox), prompt, FALSE, FALSE, 0);
  	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
  	gtk_label_set_line_wrap (GTK_LABEL (prompt), TRUE);

  	separator_line = gtk_hseparator_new ();
  	gtk_widget_show (separator_line);
  	gtk_box_pack_start (GTK_BOX (contents_vbox), separator_line, TRUE, TRUE, 8);

  	hbox_to_center_menus = gtk_hbox_new (FALSE, 0);
  	gtk_widget_show (hbox_to_center_menus);
  	gtk_box_pack_start (GTK_BOX (contents_vbox), hbox_to_center_menus, TRUE, TRUE, 0);

  	menus_vbox = gtk_vbox_new (FALSE, 4);
  	gtk_widget_show (menus_vbox);
  	gtk_box_pack_start (GTK_BOX (hbox_to_center_menus), menus_vbox, TRUE, FALSE, 0);

	for (index = 0; index < MENU_COUNT; ++index)
	{
		option_menus[index] = create_attributes_option_menu (index);
	  	gtk_option_menu_set_history (option_menus[index], index);
	  	gtk_box_pack_start (GTK_BOX (menus_vbox), GTK_WIDGET (option_menus[index]), FALSE, FALSE, 0);
		
	}

	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                    	    GTK_SIGNAL_FUNC (fm_icon_text_window_delete_event_cb),
                    	    NULL);

  	return window;
}

static gboolean
is_in_chosen_values (int value)
{
	int index;
	
	for (index = 0; index < MENU_COUNT; ++index)
	{
		if (value == get_attribute_index_from_option_menu (option_menus[index]))
		{
			return TRUE;
		}
	}

	return FALSE;
}

static void
ensure_unique_attributes (int chosen_menu)
{
	int menu_index;
	int chosen_value;

	chosen_value = get_attribute_index_from_option_menu (option_menus[chosen_menu]);

	for (menu_index = 0; menu_index < MENU_COUNT; ++menu_index)
	{
		if (menu_index == chosen_menu)
			continue;

		if (chosen_value == get_attribute_index_from_option_menu (option_menus[menu_index]))
		{
			int new_value;
			
			/* Another item already had this value; change that other item */
			for (new_value = 0; is_in_chosen_values (new_value); ++new_value)
			{}

			gtk_option_menu_set_history (option_menus[menu_index], new_value);
			return;
		}
	}
}

static gboolean
fm_icon_text_window_delete_event_cb (GtkWidget *widget,
				     GdkEvent  *event,
				     gpointer   user_data)
{
	/* Hide but don't destroy */
	gtk_widget_hide(widget);
	return TRUE;
}

/**
 * fm_icon_text_window_get_or_create:
 *
 * Get the icon text window. The first call will create the window; subsequent
 * calls will return the same window.
 * 
 * Return value: The GtkWindow with the UI for controlling which text appears
 * beneath icons.
 * 
 **/
GtkWidget *
fm_icon_text_window_get_or_create (void)
{
	static GtkWidget *icon_text_window = NULL;
	
	if (icon_text_window == NULL)
		icon_text_window = create_icon_text_window ();
	
	g_assert (GTK_IS_WINDOW (icon_text_window));
	return icon_text_window;
}
