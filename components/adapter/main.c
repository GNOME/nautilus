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

#include "nautilus-adapter-factory-server.h"
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-ui-main.h>
#include <eel/eel-debug.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <stdlib.h>
#include <string.h>

#define META_FACTORY_IID "OAFIID:nautilus_adapter_factory_generic_factory:8e62e106-807d-4d37-b14a-00dc82ecf88f"
#define FACTORY_OBJECT_IID    "OAFIID:nautilus_adapter_factory:fd24ecfc-0a6e-47ab-bc53-69d7487c6ad4"

static int object_count = 0;

static void
adapter_factory_object_weak_notify (gpointer data, GObject *object)
{
	object_count--;
	if (object_count <= 0) {
		bonobo_main_quit ();
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

	adapter = NAUTILUS_ADAPTER_FACTORY_SERVER (g_object_new (NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER, NULL));

	/* Connect a handler that will get us out of the main loop
         * when there are no more objects outstanding.
	 */
	object_count++;
	g_object_weak_ref (G_OBJECT (adapter), 
			   adapter_factory_object_weak_notify,
			   NULL);

	return BONOBO_OBJECT (adapter);
}

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	char *registration_id;

	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
	if (!bonobo_ui_init ("nautilus-adapter", VERSION, &argc, argv)) {
		g_error (_("bonobo_ui_init() failed."));
	}

	/* Disable session manager connection */
	g_object_set (G_OBJECT (gnome_program_get()),
	              GNOME_CLIENT_PARAM_SM_CONNECT, FALSE, NULL);

	/* Create the factory. */

	registration_id = bonobo_activation_make_registration_id (META_FACTORY_IID, g_getenv ("DISPLAY"));

	factory = bonobo_generic_factory_new (registration_id, adapter_factory_make_object, NULL);

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
