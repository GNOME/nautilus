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

static int object_count =0;

static void
summary_object_destroyed (GtkObject *obj) {
	object_count--;
	if (object_count <= 0) {
		gtk_main_quit ();
	}
}

static BonoboObject*
summary_make_object (BonoboGenericFactory	*factory, 
		     const char			*iid,
		     void			*closure) {

	NautilusSummaryView* view;
	NautilusView* nautilus_view;

	if (strcmp (iid, "OAFIID:nautilus_summary_view:92811b0e-beb2-49db-858c-19a0dc8517e5")) {
		return NULL;
	}

	view = NAUTILUS_SUMMARY_VIEW (gtk_object_new (NAUTILUS_TYPE_SUMMARY_VIEW, NULL));

	object_count++;

	gtk_signal_connect (GTK_OBJECT (view), "destroy", summary_object_destroyed, NULL);

	nautilus_view = nautilus_summary_view_get_nautilus_view (view);
	
	printf ("Returning new object %p\n", nautilus_view);

	return BONOBO_OBJECT (nautilus_view);
}

int
main (int argc, char *argv[]) {

	BonoboGenericFactory	*factory;
	CORBA_ORB		orb;

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
        gnome_init_with_popt_table ("nautilus-summary-view", VERSION, 
                                    argc, argv,
                                    oaf_popt_options, 0, NULL);

	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
	factory = bonobo_generic_factory_new_multi ("OAFIID:nautilus_summary_view_factory:1b0b1018-e0ca-4f14-8d23-7a134486ab30", summary_make_object, NULL);
	
	do {
		bonobo_main ();
	} while (object_count > 0);

	return 0;
}
