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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         J Shane Culpepper <pepper@eazel.com>
 */

/* main.c - main function and object activation function for services
   content view component. */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include "nautilus-summary-view.h"
#include <gconf/gconf.h>
#include <libtrilobite/libammonite.h>
#include <libtrilobite/trilobite-core-messaging.h>

static int object_count =0;

static void
summary_object_destroyed (GtkObject *obj)
{
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject*
summary_make_object (BonoboGenericFactory	*factory, 
		     const char			*iid,
		     void			*closure)
{

	NautilusSummaryView* view;
	NautilusView* nautilus_view;

	if (strcmp (iid, "OAFIID:nautilus_summary_view:92811b0e-beb2-49db-858c-19a0dc8517e5")) {
		return NULL;
	}

	view = NAUTILUS_SUMMARY_VIEW (gtk_object_new (NAUTILUS_TYPE_SUMMARY_VIEW, NULL));

	object_count++;

	nautilus_view = nautilus_summary_view_get_nautilus_view (view);
	
	gtk_signal_connect (GTK_OBJECT (nautilus_view), "destroy", summary_object_destroyed, NULL);

	printf ("Returning new object %p\n", nautilus_view);

	return BONOBO_OBJECT (nautilus_view);
}

int
main (int argc, char *argv[])
{

	BonoboGenericFactory	*factory;
	CORBA_ORB		orb;
	char *registration_id;

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif	
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

        gnome_init ("nautilus-summary-view", VERSION, 
		    argc, argv);

	gdk_rgb_init ();
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	gconf_init (argc, argv, NULL);

	ammonite_init (bonobo_poa());

	trilobite_set_log_handler (NULL, G_LOG_DOMAIN);

        registration_id = oaf_make_registration_id ("OAFIID:nautilus_summary_view_factory:1b0b1018-e0ca-4f14-8d23-7a134486ab30", getenv ("DISPLAY"));

	factory = bonobo_generic_factory_new_multi (registration_id, 
						    summary_make_object,
						    NULL);

	g_free (registration_id);

	do {
		bonobo_main ();
	} while (object_count > 0);

	return 0;
}
