/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
   Copyright (C) 2000 Eazel

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

   Author: Mathieu Lacage <mathieu@eazel.com>
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-main.h>
#include <liboaf/oaf-mainloop.h>

#define IID "OAFIID:bonobo_calculator:fab8c2a7-9576-437c-aa3a-a8617408970f"

static void
activation_callback (NautilusBonoboActivationHandle *handle,
		     Bonobo_Unknown activated_object,
		     gpointer callback_data)
{
	GtkWidget *window;
	GtkWidget *control;

	window = GTK_WIDGET (callback_data);

	if (activated_object == CORBA_OBJECT_NIL) {
		g_print ("activation failed\n");
	} else {
		control = bonobo_widget_new_control_from_objref (activated_object,
								 CORBA_OBJECT_NIL);
		gtk_container_add (GTK_CONTAINER (window), control);
		gtk_widget_show (GTK_WIDGET (control));
		
		g_print ("activation suceeded\n");
	}
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	NautilusBonoboActivationHandle *handle;

	gtk_init (&argc, &argv);
	oaf_init (argc, argv);
	bonobo_init (bonobo_orb (), 
		     bonobo_poa (), 
		     bonobo_poa_manager ());

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    gtk_main_quit, NULL);
	gtk_widget_show_all (GTK_WIDGET (window));
	
	
	handle = nautilus_bonobo_activate_from_id (IID, activation_callback, window);
#if 0
	nautilus_bonobo_activate_stop (handle);
#endif
	bonobo_main ();

	return 0;
}
