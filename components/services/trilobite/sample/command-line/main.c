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

#include "sample-service.h"

#define OAF_ID "OAFIID:trilobite_eazel_sample_service:3d972cc6-d42d-4669-bd42-966998b3c306"


CORBA_Environment ev;
CORBA_ORB orb;

/* This is basically ripped from empty-client */

int main(int argc, char *argv[]) {
	BonoboObjectClient *service;
	Trilobite_Service trilobite;
	Trilobite_Eazel_Sample sample_service; 

	CORBA_exception_init (&ev);
	gnome_init_with_popt_table ("trilobite-eazel-sample-client", "1.0",argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);

	if (bonobo_init (NULL, NULL, NULL) == FALSE) {
		g_error ("Could not init bonobo");
	}
	bonobo_activate ();

	service = bonobo_object_activate (OAF_ID, 0);
	if (!service) {
		g_error ("Cannot activate %s\n",OAF_ID);
	}

	trilobite = bonobo_object_corba_objref (BONOBO_OBJECT (service));

	/* Check the type of the object */
	g_message ("CORBA Object properties :\ncorba object\t%s\nbonobo unknown\t%s\ntrilobite\t%s\ntestservice\t%s\n",	       
		   CORBA_Object_is_a (trilobite, "IDL:CORBA/Object:1.0", &ev)?"yes":"no",
		   CORBA_Object_is_a (trilobite, "IDL:Bonobo/Unknown:1.0", &ev)?"yes":"no",
		   CORBA_Object_is_a (trilobite, "IDL:Trilobite/Service:1.0", &ev)?"yes":"no",
		   CORBA_Object_is_a (trilobite, "IDL:Trilobite/Eazel/Sample:1.0", &ev)?"yes":"no");
	

	/* Check the type of the object again... */
	g_message ("BONOBO Object properties :\ncorba object\t%s\nbonobo unknown\t%s\ntrilobite\t%s\ntestservice\t%s\n",	       
		   bonobo_object_client_has_interface (service, "IDL:CORBA/Object:1.0", &ev)?"yes":"no",
		   bonobo_object_client_has_interface (service, "IDL:Bonobo/Unknown:1.0", &ev)?"yes":"no",
		   bonobo_object_client_has_interface (service, "IDL:Trilobite/Service:1.0", &ev)?"yes":"no",
		   bonobo_object_client_has_interface (service, "IDL:Trilobite/Eazel/Sample:1.0", &ev)?"yes":"no");

	/* If a trilobite, get the interface and dump the info */
	if (bonobo_object_client_has_interface (service, "IDL:Trilobite/Service:1.0", &ev)) {
		trilobite = bonobo_object_query_interface (BONOBO_OBJECT (service), "IDL:Trilobite/Service:1.0");
		g_message ("service name        : %s", Trilobite_Service_get_name (trilobite, &ev));
		g_message ("service version     : %s", Trilobite_Service_get_version (trilobite, &ev));
		g_message ("service vendor name : %s", Trilobite_Service_get_vendor_name (trilobite, &ev));
		g_message ("service vendor url  : %s", Trilobite_Service_get_vendor_url (trilobite, &ev));
		g_message ("service url         : %s", Trilobite_Service_get_url (trilobite, &ev));
		g_message ("service icon        : %s", Trilobite_Service_get_icon (trilobite, &ev));
		
		Trilobite_Service_unref (trilobite, &ev);
	} else {
		g_warning ("Object does not support IDL:/Trilobite/Service:1.0");
	}

	/* If a test server, call the two methods */
	if (bonobo_object_client_has_interface (service, "IDL:Trilobite/Eazel/Sample:1.0", &ev)) {
		sample_service = bonobo_object_query_interface (BONOBO_OBJECT (service), "IDL:Trilobite/Eazel/Sample:1.0");

		Trilobite_Eazel_Sample_remember (sample_service, "horsedung", &ev);
		Trilobite_Eazel_Sample_say_it (sample_service, &ev);
	} 

	Trilobite_Service_unref (trilobite, &ev);
	CORBA_exception_free (&ev);

	return 0;
};
