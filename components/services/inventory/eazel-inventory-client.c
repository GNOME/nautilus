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
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <libtrilobite/libtrilobite.h>
#include <libtrilobite/libtrilobite-service.h>

#include <unistd.h>

#include "eazel-inventory.h"

#define SERVICE_IID "OAFIID:trilobite_inventory_service:eaae1152-1551-43d5-a764-52274131a9d5"


CORBA_Environment ev;
CORBA_ORB orb;


static const char *
gboolean_to_yes_or_no (gboolean bool)
{
	return bool ? "YES" : "NO";
}

static void
callback (EazelInventory *inventory,
	  gboolean succeeded,
	  gpointer callback_data)
{
	puts (succeeded ? "Upload succeeded" : "Upload failed");
	gtk_main_quit ();
}

int
main (int argc, 
      char *argv[]) 
{
	EazelInventory *inventory_service; 
	gboolean enable = FALSE;
	gboolean disable = FALSE;
	gboolean info = FALSE;
	gboolean upload = FALSE;

	struct poptOption options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		{ "enable", 'e', POPT_ARG_NONE, &enable, 0, N_("Enable inventory upload."), NULL },
#endif
		{ "disable", 'd', POPT_ARG_NONE, &disable, 0, N_("Disable inventory upload."), NULL },
		{ "info", 'i', POPT_ARG_NONE, &info, 0, N_("Display information about current inventory settings."), NULL },
		{ "upload", 'u', POPT_ARG_NONE, &upload, 0, N_("Upload inventory now, if not up to date."), NULL },
		/* FIXME bugzilla.eazel.com 5510: These OAF options don't get translated for some reason. */
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	gtk_init (&argc, &argv);

	if (enable && disable) {
		g_error ("Cannot both enable and disable inventory.");
		exit (1);
	}

	if (!trilobite_init ("eazel-inventory-client", "0.1", NULL, options, argc, argv)) {
		g_error ("Could not initialize trilobite.");
		exit (1);
	}

	CORBA_exception_init (&ev);


	/* Disable session manager connection */
	gnome_client_disable_master_connection ();


	bonobo_activate ();

	inventory_service = eazel_inventory_get ();

	if (enable) {
		eazel_inventory_set_enabled (inventory_service, TRUE);
	}

	if (disable) {
		eazel_inventory_set_enabled (inventory_service, CORBA_FALSE);
	}

	if (upload) {
		eazel_inventory_upload (inventory_service, 
					callback,
					NULL);
		gtk_main ();
	}

	if (info) {
		printf ("Inventory upload enabled: %s\n", gboolean_to_yes_or_no 
			(eazel_inventory_get_enabled (inventory_service)));
		printf ("Machine ID:               %s\n", 
			eazel_inventory_get_machine_id (inventory_service));
		/* Last upload date? */
	}

	gtk_object_unref (GTK_OBJECT (inventory_service));

	/* And free the exception structure */
	CORBA_exception_free (&ev);

	return 0;
};







