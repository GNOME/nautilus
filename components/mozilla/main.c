/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/* main.c - main function and object activation function for mozilla
   content view component. */

#include <config.h>

#include "nautilus-mozilla-content-view.h"

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <gconf/gconf.h>

#ifdef EAZEL_SERVICES
#include <libtrilobite/libammonite-gtk.h>
#endif

#include <stdlib.h>

#define nopeDEBUG_mfleming 1

static int object_count = 0;

static void
mozilla_object_destroyed (GtkObject *obj)
{
	object_count--;

#ifdef DEBUG_mfleming
	g_print ("mozilla_object_destroyed\n");
#endif

	if (object_count <= 0) {
#ifdef DEBUG_mfleming
	g_print ("...final mozilla_object_destroyed, quiting\n");
#endif

		gtk_main_quit ();
	}
}

static BonoboObject *
mozilla_make_object (BonoboGenericFactory *factory, 
		     const char *goad_id, 
		     void *closure)
{
	NautilusMozillaContentView *view;
	NautilusView *nautilus_view;

	if (strcmp (goad_id, "OAFIID:nautilus_mozilla_content_view:1ee70717-57bf-4079-aae5-922abdd576b1")) {
		return NULL;
	}

#ifdef DEBUG_mfleming
	g_print ("+mozilla_make_object\n");
#endif
	
	view = NAUTILUS_MOZILLA_CONTENT_VIEW (gtk_object_new (NAUTILUS_TYPE_MOZILLA_CONTENT_VIEW, NULL));

	object_count++;

	nautilus_view = nautilus_mozilla_content_view_get_nautilus_view (view);

	gtk_signal_connect (GTK_OBJECT (nautilus_view), "destroy", mozilla_object_destroyed, NULL);

#ifdef DEBUG_mfleming
	g_print ("-mozilla_make_object\n");
#endif

	return BONOBO_OBJECT (nautilus_view);
}

extern gboolean test_make_full_uri_from_relative (void);

static gboolean
run_test_cases (void)
{
	return test_make_full_uri_from_relative ();
}

#ifdef EAZEL_SERVICES
/*Defined in nautilus-mozilla-content-view.c*/
extern EazelProxy_UserControl nautilus_mozilla_content_view_user_control;
#endif

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	char *registration_id;
	GError *error_gconf = NULL;
	char *fake_argv[] = { "nautilus-mozilla-content-view", NULL };

#ifdef DEBUG_mfleming
	g_print ("nautilus-mozilla-content-view: starting...\n");
#endif

	if (argc == 2 && 0 == strcmp (argv[1], "--self-test")) {
		gboolean success;

		success = run_test_cases();

		exit (success ? 0 : -1);
	}
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnome_init_with_popt_table ("nautilus-mozilla-content-view", VERSION, 
				    argc, argv,
				    oaf_popt_options, 0, NULL); 
	gdk_rgb_init ();
	
	orb = oaf_init (argc, argv);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* the fake_argv thing is just superstition.  I just don't
	 * want gconf mucking with my args
	 */
	if (!gconf_init (1, fake_argv, &error_gconf)) {
		g_warning ("Couldn't init gconf");
	}
	
        registration_id = oaf_make_registration_id ("OAFIID:nautilus_mozilla_content_view_factory:020a0285-6b96-4685-84a1-4a56eb6baa2b", getenv ("DISPLAY"));
	factory = bonobo_generic_factory_new_multi (registration_id, 
						    mozilla_make_object,
						    NULL);
	g_free (registration_id);
	
	gnome_vfs_init ();

#ifdef EAZEL_SERVICES
	if (ammonite_init ((PortableServer_POA) bonobo_poa)) {
		nautilus_mozilla_content_view_user_control = ammonite_get_user_control ();
	}
#endif

#ifdef DEBUG_mfleming
	g_print ("nautilus-mozilla-content-view: OAF registration complete.\n");
#endif

	do {
		bonobo_main ();
	} while (object_count > 0);

        gnome_vfs_shutdown ();

	return 0;
}
