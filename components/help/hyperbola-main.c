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
#include <bonobo-activation/bonobo-activation.h>

#include "hyperbola-nav.h"

#define IID "OAFIID:hyperbola_factory:02b54c63-101b-4b27-a285-f99ed332ecdb"

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
		g_signal_connect (retval, "destroy",
				    G_CALLBACK (do_destroy), NULL);
	}

	return retval;
}

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
#ifdef GNOME2_CONVERSION_COMPLETE
	char *registration_id;
#endif

	/* Initialize gettext support */
#ifdef ENABLE_NLS		/* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);
#endif

	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}

	/* Disable session manager connection */
#ifdef GNOME2_CONVERSION_COMPLETE
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (bonobo_activation_popt_options,
				      bonobo_activation_get_popt_table_name ());
	orb = bonobo_activation_init (argc, argv);

	gnome_init ("hyperbola", VERSION, argc, argv);

	gdk_rgb_init ();

	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
#endif
	bonobo_ui_init ("hyperbola", VERSION, &argc, argv);

	nautilus_global_preferences_init ();

#ifdef GNOME2_CONVERSION_COMPLETE
	registration_id =
		bonobo_activation_make_registration_id
		("OAFIID:hyperbola_factory:02b54c63-101b-4b27-a285-f99ed332ecdb",
		 g_getenv ("DISPLAY"));
#endif
	factory =
		bonobo_generic_factory_new (IID, make_obj,
						  NULL);
#ifdef GNOME2_CONVERSION_COMPLETE
	g_free (registration_id);
#endif

	do {
		bonobo_main ();
	} while (object_count > 0);

	return 0;
}
