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
#include <eel/eel-image.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktogglebutton.h>

/* UTTER HACK */
static ORBit_MessageValidationResult
accept_all_cookies (CORBA_unsigned_long request_id,
		    CORBA_Principal *principal,
		    CORBA_char *operation)
{
	/* allow ALL cookies */
	return ORBIT_MESSAGE_ALLOW_ALL;
}


typedef struct
{
	char *preference_name;
	GtkWidget *button;
} Foo;

static void
preference_toggle_destroy_callback (GtkObject *object, gpointer callback_data)
{
	Foo *foo = callback_data;
	
	g_return_if_fail (foo != NULL);
	g_return_if_fail (foo->preference_name != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (foo->button));

	g_free (foo->preference_name);
	g_free (foo);
}

static void
boolean_preference_changed_callback (gpointer callback_data)
{
	Foo *foo = callback_data;
	
	g_return_if_fail (foo != NULL);
	g_return_if_fail (foo->preference_name != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (foo->button));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (foo->button),
				      eel_preferences_get_boolean (foo->preference_name));
}

static void
button_toggled (GtkWidget *button,
		gpointer callback_data)
{
	Foo *foo = callback_data;
	
	g_return_if_fail (foo != NULL);
	g_return_if_fail (foo->preference_name != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (foo->button));

	eel_preferences_set_boolean (foo->preference_name, GTK_TOGGLE_BUTTON (foo->button)->active);
}

static GtkWidget *
boolean_toggle_button_new (const char *preference_name,
			   const char *button_label)
{
	Foo *foo;
	GtkWidget *button;

	g_return_val_if_fail (preference_name != NULL, NULL);
	g_return_val_if_fail (button_label != NULL, NULL);

	
	button = gtk_toggle_button_new_with_label (button_label);
	eel_gtk_label_make_smaller (GTK_LABEL (GTK_BIN (button)->child), 4);

	foo = g_new (Foo, 1);
	foo->preference_name = g_strdup (preference_name);
	foo->button = button;

	gtk_signal_connect (GTK_OBJECT (button),
			    "destroy",
			    GTK_SIGNAL_FUNC (preference_toggle_destroy_callback),
			    foo);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      eel_preferences_get_boolean (preference_name));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (button),
					"toggled",
					GTK_SIGNAL_FUNC (button_toggled),
					foo,
					GTK_OBJECT (button));
	
	eel_preferences_add_callback_while_alive (preference_name,
						       boolean_preference_changed_callback,
						       foo,
						       GTK_OBJECT (button));

	return button;
}

static void
quit_nautilus_button_clicked_callback (GtkWidget *button,
			      gpointer callback_data)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	
	eel_gnome_shell_execute ("nautilus --quit");
}

static void
start_nautilus_button_clicked_callback (GtkWidget *button,
			      gpointer callback_data)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	
	eel_gnome_shell_execute ("nautilus");
}

static void
restart_nautilus_button_clicked_callback (GtkWidget *button,
			      gpointer callback_data)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	
	eel_gnome_shell_execute ("nautilus --restart");
}

static void
exit_button_clicked_callback (GtkWidget *button,
			      gpointer callback_data)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	
	gtk_main_quit ();
}

static void
user_level_changed_callback (gpointer callback_data)
{
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (callback_data));

	gtk_widget_set_sensitive (GTK_WIDGET (callback_data), 
				  eel_preferences_get_user_level () > 0);
}

int
main (int argc, char **argv)
{
	GtkWidget *applet;
	GtkWidget *main_hbox;
	GtkWidget *preference_vbox;
	GtkWidget *command_hbox;
	GtkWidget *show_desktop_button;
	GtkWidget *smooth_graphics_button;
	GtkWidget *quit_button;
	GtkWidget *start_button;
	GtkWidget *restart_button;
	GtkWidget *exit_button;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	applet_widget_init ("nautilus_preferences_applet",
			    VERSION,
			    argc,
			    argv,
			    NULL,
			    0,
			    NULL);

	/* want to accept all corba messages so we setup the request validator
	 * to just "accept all".  With Orbit 5.1 and higher this should be
	 * secure */
	ORBit_set_request_validation_handler (accept_all_cookies);

	applet = applet_widget_new ("nautilus_preferences_applet");

	if (applet == NULL) {
		g_error (_("Can't create nautilus-preferences-applet!"));
		exit (1);
	}

	nautilus_global_preferences_initialize ();

	main_hbox = gtk_hbox_new (FALSE, 1);

	preference_vbox = gtk_vbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (main_hbox), preference_vbox, TRUE, TRUE, 1);

	command_hbox = gtk_hbox_new (FALSE, 1);
	gtk_box_pack_start (GTK_BOX (main_hbox), command_hbox, TRUE, TRUE, 1);
	
	show_desktop_button = boolean_toggle_button_new (NAUTILUS_PREFERENCES_SHOW_DESKTOP,
							 _("Show Desktop"));
	gtk_box_pack_start (GTK_BOX (preference_vbox), show_desktop_button, TRUE, TRUE, 1);

	smooth_graphics_button = boolean_toggle_button_new (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
							    _("Smooth Graphics"));
	gtk_box_pack_start (GTK_BOX (preference_vbox), smooth_graphics_button, TRUE, TRUE, 1);

	eel_preferences_add_callback ("user_level",
					   user_level_changed_callback,
					   show_desktop_button);
	user_level_changed_callback (show_desktop_button);

	eel_preferences_add_callback ("user_level",
					   user_level_changed_callback,
					   smooth_graphics_button);
	user_level_changed_callback (smooth_graphics_button);

	quit_button = gtk_button_new_with_label (_("Quit"));
	eel_gtk_label_make_smaller (GTK_LABEL (GTK_BIN (quit_button)->child), 4);
	gtk_box_pack_start (GTK_BOX (command_hbox), quit_button, TRUE, TRUE, 1);
	gtk_signal_connect (GTK_OBJECT (quit_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (quit_nautilus_button_clicked_callback),
			    NULL);

	start_button = gtk_button_new_with_label (_("Start"));
	eel_gtk_label_make_smaller (GTK_LABEL (GTK_BIN (start_button)->child), 4);
	gtk_box_pack_start (GTK_BOX (command_hbox), start_button, TRUE, TRUE, 1);
	gtk_signal_connect (GTK_OBJECT (start_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (start_nautilus_button_clicked_callback),
			    NULL);

	restart_button = gtk_button_new_with_label (_("Restart"));
	eel_gtk_label_make_smaller (GTK_LABEL (GTK_BIN (restart_button)->child), 4);
	gtk_box_pack_start (GTK_BOX (command_hbox), restart_button, TRUE, TRUE, 1);
	gtk_signal_connect (GTK_OBJECT (restart_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (restart_nautilus_button_clicked_callback),
			    NULL);

	exit_button = gtk_button_new_with_label ("[x]");
	eel_gtk_label_make_smaller (GTK_LABEL (GTK_BIN (exit_button)->child), 4);
	gtk_box_pack_start (GTK_BOX (command_hbox), exit_button, TRUE, TRUE, 1);
	gtk_signal_connect (GTK_OBJECT (exit_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (exit_button_clicked_callback),
			    NULL);

	gtk_container_add (GTK_CONTAINER (applet), main_hbox);

	gtk_widget_show_all (applet);

	applet_widget_gtk_main ();

	return 0;
}
