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

#include <libtrilobite/libtrilobite-service.h>

#include "trilobite-eazel-time-service.h"
#include <trilobite-eazel-time-service-public.h>

#define OAF_ID_FACTORY "OAFIID:trilobite_eazel_time_service_factory:1acc6ab1-f79f-4c8d-ab87-04982fc8c19f"
#define OAF_ID "OAFIID:trilobite_eazel_time_service:13a2dbd9-84f9-4400-bd9e-bb4575b86894"
/*
  These are some generally needed objects to get CORBA connectivity
*/
CORBA_ORB                 orb;
CORBA_Environment         ev;

static BonoboGenericFactory *factory;
static int trilobites_active = 0;

static void
trilobite_service_factory_destroy (GtkObject *object) 
{
	trilobites_active--;

	g_message ("in trilobite_service_factory_destroy");
	if (trilobites_active == 0) {
		g_message ("out trilobite_service_factory_destroy (no more trilobites)");
		return;
	}
	
	bonobo_object_unref (BONOBO_OBJECT (factory));
	gtk_main_quit ();
	g_message ("out trilobite_service_factory_destroy");
}

static BonoboObject*
trilobite_eazel_time_service_factory (BonoboGenericFactory *this_factory, 
				  const gchar *oaf_id,
				  gpointer data) 
{
	TrilobiteService *trilobite;
	TrilobiteEazelTimeService *service;
	TrilobitePasswordQuery *trilobite_password;

	if (strcmp (oaf_id, OAF_ID)) {
		g_warning ("Unhandled OAF id %s", oaf_id);
		return NULL;
	}

	trilobite = TRILOBITE_SERVICE (gtk_object_new (TRILOBITE_TYPE_SERVICE,
						       "name", "Time",
						       "version", "0.3",
						       "vendor_name", "Eazel, inc.",
						       "vendor_url", "http://www.eazel.com",
						       "url", "http://testmachine.eazel.com:8888/examples/time/current/",
						       "icon", "file:///gnome/share/pixmaps/gnome-default-dlg.png",
						       NULL));

	trilobite_password = TRILOBITE_PASSWORDQUERY (gtk_object_new (TRILOBITE_TYPE_PASSWORDQUERY,
								      "prompt", "root password",
								      NULL));

	service = trilobite_eazel_time_service_new ();

	g_assert (trilobite != NULL);
	g_assert (service != NULL);

	trilobites_active++;

	trilobite_service_add_interface (trilobite, BONOBO_OBJECT (service));
	trilobite_passwordquery_add_interface (trilobite_password, BONOBO_OBJECT (service));

	gtk_signal_connect (GTK_OBJECT (trilobite),
			    "destroy",
			    trilobite_service_factory_destroy, NULL);
	
	return BONOBO_OBJECT (trilobite);
}

int main(int argc, char *argv[]) {

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	gnome_init_with_popt_table ("trilobite-eazel-time-service-factory", "0.1", argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo");
	}

	factory = bonobo_generic_factory_new_multi (OAF_ID_FACTORY, 
						    trilobite_eazel_time_service_factory,
						    NULL);
	
	if (factory == NULL) {
		g_error ("Could not register factory");
	}

	bonobo_activate();

	do {
		bonobo_main ();
	} while (trilobites_active > 0);

	CORBA_exception_free (&ev);

	return 0;
};
