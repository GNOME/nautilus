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

static gboolean fm_icon_text_window_delete_event_cb (GtkWidget *widget,
				  		     GdkEvent  *event,
				  		     gpointer   user_data);


static FMDirectoryViewIcons *icon_view = NULL;
static GtkOptionMenu *option_menu_2nd_line = NULL;
static GtkOptionMenu *option_menu_3rd_line = NULL;
static GtkOptionMenu *option_menu_4th_line = NULL;

static char * attribute_names[] = {
	"size",
	"type",
	"date_modified",
	"date_changed",
	"date_accessed",
	NULL
};

static char * attribute_labels[] = {
	_("size"),
	_("type"),
	_("date modified"),
	_("date changed"),
	_("date accessed"),
	NULL
};

#define PIECES_COUNT	4

static char *
get_chosen_attribute_name (GtkOptionMenu *option_menu)
{
	int attribute_index;
	
	g_assert (GTK_IS_OPTION_MENU (option_menu));

	attribute_index = GPOINTER_TO_INT (gtk_object_get_user_data (
		GTK_OBJECT (option_menu->menu_item)));

	return g_strdup (attribute_names[attribute_index]);
}

static void
changed_attributes_option_menu_cb (GtkMenuItem *menu_item, gpointer user_data)
{
	char ** attribute_names_array;
	char * attribute_names_string;
	
	g_assert (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));

  	attribute_names_array = g_new0 (gchar*, PIECES_COUNT + 1);

	/* Always start with the name. */
	attribute_names_array[0] = g_strdup ("name");	
	attribute_names_array[1] = get_chosen_attribute_name (option_menu_2nd_line);
	attribute_names_array[2] = get_chosen_attribute_name (option_menu_3rd_line);
	attribute_names_array[3] = get_chosen_attribute_name (option_menu_4th_line);

	attribute_names_string = g_strjoinv ("|", attribute_names_array);

	fm_directory_view_icons_set_full_icon_text_attribute_names (icon_view, 
								    attribute_names_string);

	g_free (attribute_names_string);
	g_strfreev (attribute_names_array);
}

static GtkOptionMenu *
create_attributes_option_menu (void)
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
	  			      NULL);
		
	}
  	  	
  	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	return GTK_OPTION_MENU (option_menu);
}

static GtkWidget*
create_icon_text_window ()
{
  	GtkWidget *window;
  	GtkWidget *vbox1;
  	GtkWidget *prompt;
  	GtkWidget *hseparator1;
  	GtkWidget *hbox1;
  	GtkWidget *vbox2;
  	GtkWidget *name_label;

  	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus: Icon Text"));
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);

  	vbox1 = gtk_vbox_new (FALSE, 0);
  	gtk_widget_show (vbox1);
  	gtk_container_add (GTK_CONTAINER (window), vbox1);

  	prompt = gtk_label_new (_("Choose the order for information to appear beneath icons. More information appears as you zoom in closer."));
  	gtk_widget_show (prompt);
  	gtk_box_pack_start (GTK_BOX (vbox1), prompt, FALSE, FALSE, 0);
  	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
  	gtk_label_set_line_wrap (GTK_LABEL (prompt), TRUE);

  	hseparator1 = gtk_hseparator_new ();
  	gtk_widget_show (hseparator1);
  	gtk_box_pack_start (GTK_BOX (vbox1), hseparator1, TRUE, TRUE, 8);

  	hbox1 = gtk_hbox_new (FALSE, 0);
  	gtk_widget_show (hbox1);
  	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, TRUE, TRUE, 0);

  	vbox2 = gtk_vbox_new (FALSE, 4);
  	gtk_widget_show (vbox2);
  	gtk_box_pack_start (GTK_BOX (hbox1), vbox2, TRUE, FALSE, 0);

  	name_label = gtk_label_new (_("name"));
  	gtk_widget_show (name_label);
  	gtk_box_pack_start (GTK_BOX (vbox2), name_label, FALSE, FALSE, 0);
  	gtk_misc_set_padding (GTK_MISC (name_label), 0, 4);

	option_menu_2nd_line = create_attributes_option_menu ();
  	gtk_box_pack_start (GTK_BOX (vbox2), GTK_WIDGET (option_menu_2nd_line), FALSE, FALSE, 0);

  	option_menu_3rd_line = create_attributes_option_menu ();
  	gtk_option_menu_set_history (option_menu_3rd_line, 1);
  	gtk_box_pack_start (GTK_BOX (vbox2), GTK_WIDGET (option_menu_3rd_line), FALSE, FALSE, 0);

  	option_menu_4th_line = create_attributes_option_menu ();
  	gtk_box_pack_start (GTK_BOX (vbox2), GTK_WIDGET (option_menu_4th_line), FALSE, FALSE, 0);
  	gtk_option_menu_set_history (option_menu_4th_line, 2);

	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                    	    GTK_SIGNAL_FUNC (fm_icon_text_window_delete_event_cb),
                    	    NULL);

  	return window;
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

/**
 * fm_icon_text_window_set_view:
 *
 * Specify which directory view the icon text window is controlling.
 * 
 **/
void
fm_icon_text_window_set_view (GtkWindow *window, FMDirectoryViewIcons *view)
{
	/* FIXME: Implement this or remove the API for it. Note that there
	 * are tricky UI issues since the window is (at least currently)
	 * non-modal but it affects a particular directory window. Must at
	 * minimum make that obvious in the icon text window. User can
	 * change this by choosing "Customize icon text..." in a different
	 * directory, but that's pretty subtle.
	 */

	g_return_if_fail (FM_IS_DIRECTORY_VIEW_ICONS (view));
	icon_view = view;	 
}