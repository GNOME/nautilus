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

#include "eazel-inventory-service-interface.h"

#define SERVICE_IID "OAFIID:trilobite_inventory_service:eaae1152-1551-43d5-a764-52274131a9d5"


CORBA_Environment ev;
CORBA_ORB orb;


static const char *
gboolean_to_yes_or_no (gboolean bool)
{
	return bool ? "YES" : "NO";
}

int
main (int argc, 
      char *argv[]) 
{
	BonoboObjectClient *service;
	Trilobite_Eazel_Inventory inventory_service; 
	gboolean enable = FALSE;
	gboolean disable = FALSE;
	gboolean enable_warn = FALSE;
	gboolean disable_warn = FALSE;
	char * machine_name=NULL;
	gboolean info = FALSE;
	gboolean upload = FALSE;


	struct poptOption options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		{ "enable", 'e', POPT_ARG_NONE, &enable, 0, N_("Enable inventory upload."), NULL },
#endif
		{ "disable", 'd', POPT_ARG_NONE, &disable, 0, N_("Disable inventory upload."), NULL },
		{ "enable-warn", 'w', POPT_ARG_NONE, &enable_warn, 0, N_("Enable warning before each upload."), NULL },
		{ "disable-warn", 'n', POPT_ARG_NONE, &disable_warn, 0, N_("Disable warning before each upload."), NULL },
		{ "machine-name", 'm', POPT_ARG_STRING, &machine_name, 0, N_("Set machine name."), NULL },
		{ "info", 'i', POPT_ARG_NONE, &info, 0, N_("Display information about current inventory settings."), NULL },
		{ "upload", 'u', POPT_ARG_NONE, &upload, 0, N_("Upload inventory now, if not up to date."), NULL },
		/* FIXME bugzilla.eazel.com 5510: These OAF options don't get translated for some reason. */
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	if (enable && disable) {
		g_error ("Cannot both enable and disable inventory.");
	}

	if (enable_warn && disable_warn) {
		g_error ("Cannot both enable and disable warning before upload.");
	}
	

	if (!trilobite_init ("eazel-inventory-client", "0.1", NULL, options, argc, argv)) {
		g_error ("Could not initialize trilobite.");
		exit (1);
	}

	CORBA_exception_init (&ev);

	bonobo_activate ();

	service = bonobo_object_activate (SERVICE_IID, 0);
	if (!service) {
		g_error ("Cannot activate %s\n", SERVICE_IID);
	}

	if (!bonobo_object_client_has_interface (service, "IDL:Trilobite/Eazel/Inventory:1.0", &ev)) {
		bonobo_object_unref (BONOBO_OBJECT (service)); 
		g_error ("Inventory component does not have inventory interface.");
	}

	inventory_service = bonobo_object_query_interface (BONOBO_OBJECT (service), "IDL:Trilobite/Eazel/Inventory:1.0");

	if (enable) {
		Trilobite_Eazel_Inventory__set_enabled (inventory_service, CORBA_TRUE, &ev);
	}

	if (disable) {
		Trilobite_Eazel_Inventory__set_enabled (inventory_service, CORBA_FALSE, &ev);
	}

	if (machine_name != NULL) {
		Trilobite_Eazel_Inventory__set_machine_name (inventory_service, machine_name, &ev);
	}

	if (enable_warn) {
		Trilobite_Eazel_Inventory__set_warn_before_upload (inventory_service, CORBA_TRUE, &ev);
	}

	if (disable_warn) {
		Trilobite_Eazel_Inventory__set_warn_before_upload (inventory_service, CORBA_FALSE, &ev);
	}

	if (upload) {
		Trilobite_Eazel_Inventory_upload (inventory_service, &ev);
	}

	if (info) {
		printf ("Inventory upload enabled: %s\n", gboolean_to_yes_or_no 
			(Trilobite_Eazel_Inventory__get_enabled (inventory_service, &ev)));
		printf ("Machine name:             %s\n", 
			Trilobite_Eazel_Inventory__get_machine_name (inventory_service, &ev));
		printf ("Warn before upload:       %s\n", gboolean_to_yes_or_no 
			(Trilobite_Eazel_Inventory__get_warn_before_upload (inventory_service, &ev)));
		/* Last upload date? */
	}
		

	/* Clean up the bonobo_object_activate return value */
	bonobo_object_unref (BONOBO_OBJECT (service)); 
	/* And free the exception structure */
	CORBA_exception_free (&ev);

	return 0;
};







