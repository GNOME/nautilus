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
	TrilobiteService *test_service;

	/* Init the CORBA */
	CORBA_exception_init (&ev);
	gnome_init("sample-service", "0.1",argc, argv);
	orb = oaf_init (argc, argv);

	/* Create the trilobite test service corba object */
	test_service = trilobite_service_new("OAFIID:NautilusEazelSampleService:134276");

	/* keep it alive... */
	while(1) {
		g_main_iteration(TRUE);
	}


	/* This is just so that I can check strace ends here */
	return 42;
};
