/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

/*
 * A very simple applet to toggle some Nautilus preferences.
 */

#include <config.h>
#include <applet-widget.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <gtk.h>

static void
button_toggled (GtkWidget *button,
		gpointer callback_data)
{
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP,
					  GTK_TOGGLE_BUTTON (button)->active);
}

static void
show_desktop_changed_callback (gpointer callback_data)
{
	gboolean show_desktop;

	show_desktop = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (callback_data),
				      nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP));
}

static void
delete_event (GtkWidget *widget, GdkEvent *event, gpointer callback_data)
{
	gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	GtkWidget *applet;
	GtkWidget *button;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
	
	applet_widget_init ("nautilus_preferences_applet",
			    VERSION,
			    argc,
			    argv,
			    NULL,
			    0,
			    NULL);

	applet = applet_widget_new ("nautilus_preferences_applet");

	if (applet == NULL) {
		g_error (_("Can't create nautilus-preferences-applet!"));
		exit (1);
	}

	nautilus_global_preferences_initialize ();

	button = gtk_toggle_button_new_with_label ("Show Desktop");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP));
	
	gtk_signal_connect (GTK_OBJECT (button),
			    "toggled",
			    GTK_SIGNAL_FUNC (button_toggled),
			    NULL);

	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_DESKTOP, 
					   show_desktop_changed_callback, 
					   button);

	gtk_container_add (GTK_CONTAINER (applet), button);

	gtk_widget_show_all (applet);
	
// 	gtk_signal_connect(GTK_OBJECT(applet),"change_pixel_size",
// 			   GTK_SIGNAL_FUNC (applet_change_pixel_size),
// 			   NULL);

	applet_widget_gtk_main ();

	return 0;
}
