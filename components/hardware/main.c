/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Andy Hertzfeld
 */

/* main.c - main function and object activation function for the hardware view component. */

#define IID "OAFIID:nautilus_hardware_view_factory:8c80e55a-5c03-4403-9e51-3a5711b8a5ce" 

#include <config.h>

#include "nautilus-hardware-view.h"

#include <bonobo.h>
#include <eel/eel-debug.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <bonobo-activation/bonobo-activation.h>

static int object_count = 0;

static void
hardware_view_object_destroyed(GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
hardware_view_make_object (BonoboGenericFactory *factory, 
			   const char *bonobo_activation_iid, 
			   void *closure)
{
	NautilusView *view;

	if (strcmp (bonobo_activation_iid, "OAFIID:nautilus_hardware_view:4a3f3793-bab4-4640-9f56-e7871fe8e150")) {
		return NULL;
	}
	
	view = nautilus_hardware_view_get_nautilus_view (NAUTILUS_HARDWARE_VIEW (g_object_new (NAUTILUS_TYPE_HARDWARE_VIEW, NULL)));

	object_count++;

	g_signal_connect (view, "destroy", G_CALLBACK (hardware_view_object_destroyed), NULL);

	return BONOBO_OBJECT (view);
}

int main(int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_Environment ev;
#ifdef GNOME2_CONVERSION_COMPLETE
	char *registration_id;
#endif
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger (G_LOG_DOMAIN, NULL);
	}
	
	CORBA_exception_init(&ev);

#ifdef GNOME2_CONVERSION_COMPLETE
	gnomelib_register_popt_table (bonobo_activation_popt_options, bonobo_activation_get_popt_table_name ());
	orb = bonobo_activation_init (argc, argv);

        gnome_init ("nautilus-hardware-view", VERSION, 
		    argc, argv); 

	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* initialize gnome-vfs, etc */
	g_thread_init (NULL);
	gnome_vfs_init ();
#endif
	bonobo_ui_init ("nautilus-hardware-view", VERSION, &argc, argv);

#ifdef GNOME2_CONVERSION_COMPLETE
        registration_id = bonobo_activation_make_registration_id ("OAFIID:nautilus_hardware_view_factory:8c80e55a-5c03-4403-9e51-3a5711b8a5ce", 
						    getenv ("DISPLAY"));
#endif
	factory = bonobo_generic_factory_new (IID, 
						    hardware_view_make_object,
						    NULL);

#ifdef GNOME2_CONVERSION_COMPLETE
	g_free (registration_id);
#endif
	
	do {
		bonobo_main ();
	} while (object_count > 0);
	
        gnome_vfs_shutdown ();
	return 0;
}
