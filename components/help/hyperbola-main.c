/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <config.h>

#include <bonobo.h>
#include <eel/eel-debug.h>
#include <gnome.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <liboaf/liboaf.h>

#include "hyperbola-nav.h"

static int object_count = 0;

static void
do_destroy (GtkObject * obj)
{
	object_count--;

	if (object_count <= 0)
		gtk_main_quit ();
}
/*
 * If scrollkeeper support is enabled then hyperbola_navigation_tree_new()
 * is the only function called. This will create the contents display and
 * the index/search display.
 *
 * If scrollkeeper support is not enabled, then this function is called and
 * will only create the contents display. The other functions 
 * hyperbola_navigation_index_new() and hyperbola_navigation_search_new()
 * are not implemented.
 */

static BonoboObject *
make_obj (BonoboGenericFactory * Factory, const char *goad_id, void *closure)
{
	BonoboObject *retval = NULL;
	/*
         * If scrollkeeper support is enabled then hyperbola_navigation_tree_new()
	 * will create the access to the contents display  and the index/search display.
	 */
	if (!strcmp
	    (goad_id,
	     "OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234"))
			retval = hyperbola_navigation_tree_new ();
#ifndef ENABLE_SCROLLKEEPER_SUPPORT
 	else
 		if (!strcmp
 		    (goad_id,
 		     "OAFIID:hyperbola_navigation_index:0bafadc7-09f1-4f10-8c8e-dad53124fc49"))
 	       retval = hyperbola_navigation_index_new ();
 	else
 		if (!strcmp
 		    (goad_id,
 		     "OAFIID:hyperbola_navigation_search:89b2f3b8-4f09-49c8-9a7b-ccb14d034813"))
 	       retval = hyperbola_navigation_search_new ();
#endif

	if (retval) {
		object_count++;
		gtk_signal_connect (GTK_OBJECT (retval), "destroy",
				    do_destroy, NULL);
	}

	return retval;
}

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	char *registration_id;


	/* Initialize gettext support */
#ifdef ENABLE_NLS		/* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger (G_LOG_DOMAIN, NULL);
	}

	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options,
				      oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

	gnome_init ("hyperbola", VERSION, argc, argv);

	gdk_rgb_init ();

	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	nautilus_global_preferences_initialize ();

	registration_id =
		oaf_make_registration_id
		("OAFIID:hyperbola_factory:02b54c63-101b-4b27-a285-f99ed332ecdb",
		 g_getenv ("DISPLAY"));
	factory =
		bonobo_generic_factory_new_multi (registration_id, make_obj,
						  NULL);
	g_free (registration_id);


	do {
		bonobo_main ();
	} while (object_count > 0);

	return 0;
}
