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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include "eazel-inventory-service.h"

#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <eel/eel-debug.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <gconf/gconf.h>

#include <libtrilobite/libtrilobite.h>
#include <libtrilobite/libtrilobite-service.h>

#include <libtrilobite/libammonite.h> /* FIXME: crack */


#define FACTORY_IID "OAFIID:trilobite_inventory_service_factory:ddc067a1-45e1-41ae-925a-bb6ecdaf3047"
#define SERVICE_IID "OAFIID:trilobite_inventory_service:eaae1152-1551-43d5-a764-52274131a9d5"

/*
  These are some generally needed objects to get CORBA connectivity
*/
CORBA_ORB                 orb;
CORBA_Environment         ev;

static BonoboGenericFactory   *factory;
static int trilobites_active = 0;

static void
eazel_inventory_service_factory_destroy (GtkObject *object) 
{
	trilobites_active--;

	if (trilobites_active > 0) {
		return;
	}

	g_message ("in factory_destroy");
	
	bonobo_object_unref (BONOBO_OBJECT (factory));

	trilobite_main_quit ();
	g_message ("out factory_destroy");
}

static BonoboObject*
eazel_inventory_service_factory (BonoboGenericFactory *this_factory, 
				     const gchar *oaf_id,
				     gpointer data) 
{
	TrilobiteService *trilobite;
	EazelInventoryService *service;

	g_message ("in eazel_inventory_service_factory");

	if (strcmp (oaf_id, SERVICE_IID)) {
		return NULL;
	}

	trilobite = TRILOBITE_SERVICE (gtk_object_new (TRILOBITE_TYPE_SERVICE,
						       "name", "Inventory",
						       "version", "0.1",
						       "vendor_name", "Eazel, inc.",
						       "vendor_url", "http://www.eazel.com",
						       "url", "http://www.eazel.com/inventory/info",
						       "icon", "gnome-default-dlg.png",
						       NULL));

	service = eazel_inventory_service_new ();

	trilobites_active++;

	trilobite_service_add_interface (trilobite, BONOBO_OBJECT (service));

	gtk_signal_connect (GTK_OBJECT (trilobite),
			    "destroy",
			    eazel_inventory_service_factory_destroy, NULL);
	
	return BONOBO_OBJECT (trilobite);
}

int 
main (int argc, 
      char *argv[]) {

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	if (!trilobite_init ("trilobite-inventory-service", "0.1", "/tmp/eazel-inventory.log", NULL, argc, argv)) {
		g_error ("Could not initialize trilobite. :(");
		exit (1);
	}
	trilobite_set_debug_mode (FALSE);

	gnome_vfs_init ();

	/* FIXME: check for errors */
	if (!gconf_is_initialized ()) {
		gconf_init (argc, argv, NULL);
	}

	ammonite_init (bonobo_poa ());

	factory = bonobo_generic_factory_new_multi (FACTORY_IID,
						    eazel_inventory_service_factory,
						    NULL);

	if (factory == NULL) {
		g_error ("Could not register factory");
	}

	bonobo_activate();

	g_message ("%s ready", argv[0]);

	do {
		trilobite_main ();
	} while (trilobites_active > 0);

	g_message ("%s quitting", argv[0]);

	CORBA_exception_free (&ev);

	gnome_vfs_shutdown ();

	return 0;
};
