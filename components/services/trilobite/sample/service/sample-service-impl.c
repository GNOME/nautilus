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
#include "sample-service.h"
#include <trilobite-service-public.h>

/*
  These are some generally needed objects to get CORBA connectivity
*/
CORBA_ORB                 orb;
PortableServer_POA        poa;
CORBA_Environment         ev;

int main(int argc, char *argv[]) {
	TrilobiteService *trilobite;
	PortableServer_POA poa;
	poptContext ctx;
	
	gnome_init ("nautilus-service", "0.1", argc, argv);
	/* Init the CORBA */
	CORBA_exception_init (&ev);
	orb = oaf_init (argc, argv);

	ctx =
		poptGetContext ("oaf-empty-server", argc, argv,
				oaf_popt_options, 0);
	while (poptGetNextOpt (ctx) >= 0);

	poptFreeContext (ctx);

	poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references (oaf_orb_get (), "RootPOA", &ev);

	/* Create the trilobite test service corba object */
	trilobite = trilobite_service_new();
	if (!trilobite) {
		g_error ("trilobite === NULL");
	}
	trilobite_service_activate (trilobite);

	oaf_active_server_register ("OAFIID:NautilusEazelSampleService:134276", trilobite->corba_object);
	PortableServer_POAManager_activate (PortableServer_POA__get_the_POAManager (poa, &ev), &ev);

	/* keep it alive... */
	while(trilobite_service_alive (trilobite)) {
		g_main_iteration(TRUE);
	}

	trilobite_service_destroy (trilobite);

	/* This is just so that I can check strace ends here */
	return 42;
};
