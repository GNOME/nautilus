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

/* #include "sample-service.h" */
#include <trilobite-service.h>
#include <trilobite-service-public.h>

#define OAF_ID_FACTORY "OAFIID:nautilus_eazel_sample_service_factory:134276"
#define OAF_ID "OAFIID:nautilus_eazel_sample_service:134276"

/*
  These are some generally needed objects to get CORBA connectivity
*/
CORBA_ORB                 orb;
CORBA_Environment         ev;

static BonoboGenericFactory *factory;
static int trilobites_active = 0;

/*
static void
impl_Trilobite_Eazel_Sample_Service_remember(PortableServer_servant servant,
					     const CORBA_char *something,
					     CORBA_Environment *ev) 
{
}

static void
impl_Trilobite_Eazel_Sample_Service_say_it(PortableServer_servant servant,
					   CORBA_Environment *ev) 
{
}
*/

static void
trilobite_service_factory_destroy (GtkObject *object) 
{
	trilobites_active--;

	if (trilobites_active == 0) {
		return;
	}
	
	bonobo_object_unref (BONOBO_OBJECT (factory));
	gtk_main_quit ();
}

static BonoboObject*
trilobite_sample_service_factory (BonoboGenericFactory *this_factory, 
				  const gchar *oaf_id,
				  gpointer data) 
{
	TrilobiteService *trilobite;

	g_message ("in trilobite_sample_service_factory");

	if (strcmp (oaf_id, OAF_ID)) {
		return NULL;
	}

	trilobite = trilobite_service_new ();

	trilobites_active++;

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

	gnome_init_with_popt_table ("nautilus-service", "0.1", argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo");
	}

	factory = bonobo_generic_factory_new_multi (OAF_ID_FACTORY, 
						    trilobite_sample_service_factory,
						    NULL);
	
	if (factory == NULL) {
		g_error ("Could not register factory");
	}

	if (1) {
		TrilobiteService *test;
		test = trilobite_service_new ();
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
