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
#include <stdio.h>
#include <liboaf/liboaf.h>

#include "sample-service.h"
#include <trilobite-service-public.h>

CORBA_Environment ev;
CORBA_ORB orb;

/* This is basically ripped from empty-client */

int main(int argc, char *argv[]) {
	Trilobite_Service service;
	char *oafid = "OAFIID:NautilusEazelSampleService:134276";

	CORBA_exception_init (&ev);
	gnome_init ("nautilus-eazel-sample-service", "1.0",argc, argv);
	orb = oaf_init (argc, argv);

	service = oaf_activate_from_id (oafid, 0, NULL, &ev);

	if (!service) {
		g_error ("Cannot activate %s\n",oafid);
	}

	/* Check the type of the object */
	g_message ("Object properties : corba = %s trilobite = %s, testservice = %s\n",	       
		   CORBA_Object_is_a (service, "IDL:CORBA/Object:1.0", &ev)?"yes":"no",
		   CORBA_Object_is_a (service, "IDL:Trilobite/Service:1.0", &ev)?"yes":"no",
		   CORBA_Object_is_a (service, "IDL:Trilobite/Eazel/TestService:1.0", &ev)?"yes":"no");

	g_message ("service name        : %s", Trilobite_Service_get_name (service,&ev));
	g_message ("service version     : %s", Trilobite_Service_get_version (service,&ev));
	g_message ("service vendor name : %s", Trilobite_Service_get_vendor_name (service,&ev));
	g_message ("service vendor url  : %s", Trilobite_Service_get_vendor_url (service,&ev));
	g_message ("service url         : %s", Trilobite_Service_get_url (service,&ev));
	g_message ("service icon        : %s", Trilobite_Service_get_icon (service,&ev));

	Trilobite_Service_done (service,&ev);

	CORBA_Object_release (service, &ev);
	CORBA_Object_release ((CORBA_Object)orb, &ev);

	return 0;
};
