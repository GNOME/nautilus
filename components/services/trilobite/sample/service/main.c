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

#include "sample-service.h"
#include <sample-service-public.h>

#define OAF_ID_FACTORY "OAFIID:trilobite_eazel_sample_service_factory:19084a03-2f85-456e-95ed-bcebf8141e97"
#define OAF_ID "OAFIID:trilobite_eazel_sample_service:3d972cc6-d42d-4669-bd42-966998b3c306"

/*
  These are some generally needed objects to get CORBA connectivity
*/
CORBA_ORB                 orb;
CORBA_Environment         ev;

static BonoboGenericFactory *factory;
static int trilobites_active = 0;
static TrilobitePasswordQuery *trilobite_password;

static void
trilobite_service_factory_destroy (GtkObject *object) 
{
	trilobites_active--;

	if (trilobites_active == 0) {
		return;
	}

	g_message ("destroying factory");
	
	bonobo_object_unref (BONOBO_OBJECT (factory));
	trilobite_passwordquery_destroy (GTK_OBJECT (trilobite_password));

	gtk_main_quit ();
}

static BonoboObject*
trilobite_sample_service_factory (BonoboGenericFactory *this_factory, 
				  const gchar *oaf_id,
				  gpointer data) 
{
	TrilobiteService *trilobite;
	SampleService *service;

	g_message ("in trilobite_sample_service_factory");

	if (strcmp (oaf_id, OAF_ID)) {
		return NULL;
	}

	trilobite = TRILOBITE_SERVICE (gtk_object_new (TRILOBITE_TYPE_SERVICE,
						       "name", "Sample",
						       "version", "0.1",
						       "vendor_name", "Eazel, inc.",
						       "vendor_url", "http://www.eazel.com",
						       "url", "http://www.eazel.com/sample",
						       "icon", "gnome-default-dlg.png",
						       NULL));

	trilobite_password = TRILOBITE_PASSWORDQUERY (gtk_object_new (TRILOBITE_TYPE_PASSWORDQUERY, 
								      "prompt", "root", 
								      NULL));

	service = sample_service_new ();

	trilobites_active++;

	trilobite_service_add_interface (trilobite, BONOBO_OBJECT (service));
	trilobite_passwordquery_add_interface (trilobite_password, BONOBO_OBJECT (service));

	gtk_signal_connect (GTK_OBJECT (trilobite),
			    "destroy",
			    trilobite_service_factory_destroy, NULL);
	
	return BONOBO_OBJECT (trilobite);
}

int main(int argc, char *argv[]) {

	GData *data;

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	g_datalist_init (&data);
	g_datalist_set_data (&data, "debug", (void *)1);
	if (trilobite_init ("trilobite-sample-service", "0.1", "/tmp/trilobite.log", argc, argv, data) == FALSE) {
		g_error ("Could not initialize trilobite. :(");
		exit (1);
	}
	g_datalist_clear (&data);

	factory = bonobo_generic_factory_new_multi (OAF_ID_FACTORY, 
						    trilobite_sample_service_factory,
						    NULL);
	
	if (factory == NULL) {
		g_error ("Could not register factory");
	}

	bonobo_activate();

	g_message ("%s ready", argv[0]);

	do {
		bonobo_main ();
	} while (trilobites_active > 0);

	g_message ("%s quitting", argv[0]);

	CORBA_exception_free (&ev);

	return 0;
};
