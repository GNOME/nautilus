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
#include <gtk/gtkvbox.h>
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
				      nautilus_preferences_get_boolean (foo->preference_name));
}

static void
button_toggled (GtkWidget *button,
		gpointer callback_data)
{
	Foo *foo = callback_data;
	
	g_return_if_fail (foo != NULL);
	g_return_if_fail (foo->preference_name != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (foo->button));

	nautilus_preferences_set_boolean (foo->preference_name, GTK_TOGGLE_BUTTON (foo->button)->active);
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

	foo = g_new (Foo, 1);
	foo->preference_name = g_strdup (preference_name);
	foo->button = button;

	gtk_signal_connect (GTK_OBJECT (button),
			    "destroy",
			    GTK_SIGNAL_FUNC (preference_toggle_destroy_callback),
			    foo);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				      nautilus_preferences_get_boolean (preference_name));
	
	gtk_signal_connect_while_alive (GTK_OBJECT (button),
					"toggled",
					GTK_SIGNAL_FUNC (button_toggled),
					foo,
					GTK_OBJECT (button));
	
	nautilus_preferences_add_callback_while_alive (preference_name,
						       boolean_preference_changed_callback,
						       foo,
						       GTK_OBJECT (button));

	return button;
}

int
main (int argc, char **argv)
{
	GtkWidget *applet;
	GtkWidget *vbox;
	GtkWidget *show_desktop_button;
	GtkWidget *smooth_graphics_button;

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

	vbox = gtk_vbox_new (FALSE, 1);
	
	show_desktop_button = boolean_toggle_button_new (NAUTILUS_PREFERENCES_SHOW_DESKTOP,
							 "Show Desktop");

	gtk_box_pack_start (GTK_BOX (vbox), show_desktop_button, TRUE, TRUE, 1);

	smooth_graphics_button = boolean_toggle_button_new (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
							    "Smooth Graphics");
	
	gtk_box_pack_start (GTK_BOX (vbox), smooth_graphics_button, TRUE, TRUE, 1);

	gtk_container_add (GTK_CONTAINER (applet), vbox);

	gtk_widget_show_all (applet);

	applet_widget_gtk_main ();

	return 0;
}
