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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* main.c - Main function and object activation function for component adapter
 */

#include <config.h>

#include <libgnome/gnome-defs.h> /* must come before gnome-init.h */
#include <libgnomeui/gnome-init.h> /* must come before liboaf.h */

#include "nautilus-adapter-factory-server.h"
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <eel/eel-debug.h>
#include <liboaf/liboaf.h>
#include <stdlib.h>

#define META_FACTORY_IID "OAFIID:nautilus_adapter_factory_generic_factory:8e62e106-807d-4d37-b14a-00dc82ecf88f"
#define FACTORY_OBJECT_IID    "OAFIID:nautilus_adapter_factory:fd24ecfc-0a6e-47ab-bc53-69d7487c6ad4"

static int object_count = 0;

static void
adapter_factory_object_destroyed (GtkObject *object)
{
	g_assert (GTK_IS_OBJECT (object));

	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject *
adapter_factory_make_object (BonoboGenericFactory *factory, 
		    const char *iid, 
		    gpointer callback_data)
{
	NautilusAdapterFactoryServer *adapter;

	g_assert (BONOBO_IS_GENERIC_FACTORY (factory));
	g_assert (iid != NULL);
	g_assert (callback_data == NULL);

	/* Check that this is the one type of object we know how to
	 * create.
	 */
	if (strcmp (iid, FACTORY_OBJECT_IID) != 0) {
		return NULL;
	}

	adapter = NAUTILUS_ADAPTER_FACTORY_SERVER (gtk_object_new (NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER, NULL));

	/* Connect a handler that will get us out of the main loop
         * when there are no more objects outstanding.
	 */
	object_count++;
	gtk_signal_connect (GTK_OBJECT (adapter), "destroy",
			    adapter_factory_object_destroyed, NULL);

	return BONOBO_OBJECT (adapter);
}

int
main (int argc, char *argv[])
{
	CORBA_ORB orb;
	BonoboGenericFactory *factory;
	char *registration_id;

	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", "ORBit", NULL);
	}
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

	/* Initialize libraries. */
        gnome_init ("nautilus-adapter", VERSION, 
		    argc, argv); 
	g_thread_init (NULL);
	gnome_vfs_init ();
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* Create the factory. */

	registration_id = oaf_make_registration_id (META_FACTORY_IID, g_getenv ("DISPLAY"));

	factory = bonobo_generic_factory_new_multi (registration_id, adapter_factory_make_object, NULL);

	g_free (registration_id);
	
	/* Loop until we have no more objects. */
	do {
		bonobo_main ();
	} while (object_count > 0);

	/* Let the factory go. */
	bonobo_object_unref (BONOBO_OBJECT (factory));

        gnome_vfs_shutdown ();

	return EXIT_SUCCESS;
}
