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

#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>

/* Larger size initially; user can stretch or shrink (but not shrink below min) */
#define ICON_TEXT_WINDOW_INITIAL_WIDTH	500
#define ICON_TEXT_WINDOW_INITIAL_HEIGHT	200

static gboolean fm_icon_text_window_delete_event_cb (GtkWidget *widget,
				  		     GdkEvent  *event,
				  		     gpointer   user_data);


static FMDirectoryViewIcons *icon_view = NULL;

static void
toggled_radio_button (GtkToggleButton *button, gpointer user_data)
{
	char * all_attribute_names;
	
	g_assert (FM_IS_DIRECTORY_VIEW_ICONS (icon_view));

	if (!gtk_toggle_button_get_active (button))
		return;
		
	switch (GPOINTER_TO_INT (user_data))
	{
		case 0:
			all_attribute_names = "name|size|date_modified|type";
			break;
		case 1:
			all_attribute_names = "name|date_modified|type|size";
			break;
		case 2:
			all_attribute_names = "name|type|size|date_modified";
			break;
		default:
			g_assert_not_reached ();
	}

	fm_directory_view_icons_set_full_icon_text_attribute_names (icon_view, 
								    all_attribute_names,
								    TRUE);
}

static GtkWidget*
create_icon_text_window (void)
{
  	GtkWidget *window;
  	GtkWidget *prompt;
  	GtkWidget *vbox;
  	GSList *radio_group;
  	GtkWidget *default_1;
  	GtkWidget *default_2;
  	GtkWidget *custom;

  	radio_group = NULL;

  	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, FALSE, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus: Icon Text (placeholder UI)"));

  	vbox = gtk_vbox_new (FALSE, 0);
  	gtk_widget_show (vbox);
  	gtk_container_add (GTK_CONTAINER (window), vbox);

  	prompt = gtk_label_new (_("Choose the order for information to appear beneath icons.\nMore information appears as you zoom in closer."));
	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
	gtk_widget_show (prompt);
	gtk_box_pack_start (GTK_BOX (vbox), prompt, FALSE, FALSE, GNOME_PAD);

  	default_1 = gtk_radio_button_new_with_label (radio_group, _("name, size, date modified, type"));
  	radio_group = gtk_radio_button_group (GTK_RADIO_BUTTON (default_1));
  	gtk_widget_show (default_1);
  	gtk_box_pack_start (GTK_BOX (vbox), default_1, FALSE, FALSE, 0);
  	gtk_signal_connect (GTK_OBJECT (default_1),
  			      "toggled",
  			      toggled_radio_button,
  			      GINT_TO_POINTER (0));

  	default_2 = gtk_radio_button_new_with_label (radio_group, _("name, date modified, type, size"));
  	radio_group = gtk_radio_button_group (GTK_RADIO_BUTTON (default_2));
  	gtk_widget_show (default_2);
  	gtk_box_pack_start (GTK_BOX (vbox), default_2, FALSE, FALSE, 0);
  	gtk_signal_connect (GTK_OBJECT (default_2),
  			      "toggled",
  			      toggled_radio_button,
  			      GINT_TO_POINTER (1));

  	custom = gtk_radio_button_new_with_label (radio_group, _("name, type, size, date modified"));
  	radio_group = gtk_radio_button_group (GTK_RADIO_BUTTON (custom));
  	gtk_widget_show (custom);
  	gtk_box_pack_start (GTK_BOX (vbox), custom, FALSE, FALSE, 0);
  	gtk_signal_connect (GTK_OBJECT (custom),
  			      "toggled",
  			      toggled_radio_button,
  			      GINT_TO_POINTER (2));

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