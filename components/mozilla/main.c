/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <gconf/gconf.h>
#include <gtkmozembed.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <stdlib.h>

#define nopeDEBUG_mfleming 1

#ifdef DEBUG_mfleming
#define DEBUG_MSG(x)	g_print x
#else
#define DEBUG_MSG(x)	
#endif

/* Hold the process for a half hour after the last mozilla component has
 * been freed
 */ 
#define MOZILLA_QUIT_TIMEOUT_DELAY (30 * 60 * 1000)

static int object_count = 0;
static guint quit_timeout_id = 0;

static gboolean
mozilla_process_delayed_exit (gpointer data)
{
	DEBUG_MSG (("mozilla_object_delayed_exit\n"));

	if (object_count == 0) {
		DEBUG_MSG (("mozilla_object_delayed_exit: object count 0, exiting\n"));

		gtk_moz_embed_pop_startup();

		gtk_main_quit();
	}

	return FALSE;
}

static void
mozilla_object_destroyed (GtkObject *obj)
{
	static guint32 delayed_exit_interval = 0;
	object_count--;

	DEBUG_MSG (("mozilla_object_destroyed\n"));

	if (delayed_exit_interval == 0) {
		if (getenv ("NAUTILUS_MOZILLA_COMPONENT_DONT_DELAY_EXIT")) {
			delayed_exit_interval = 1;
		} else {
			delayed_exit_interval = MOZILLA_QUIT_TIMEOUT_DELAY;
		}
	}
	
	if (object_count == 0) {
		DEBUG_MSG (("mozilla_object_destroyed: 0 objects remaining, scheduling quit\n"));

		if (quit_timeout_id != 0) {
			gtk_timeout_remove (quit_timeout_id);
		}

		quit_timeout_id = gtk_timeout_add (delayed_exit_interval,
						   mozilla_process_delayed_exit,
						   NULL);
	}
}

static BonoboObject *
mozilla_make_object (BonoboGenericFactory *factory, 
		     const char *goad_id, 
		     void *closure)
{
	BonoboObject *bonobo_object;

	if (strcmp (goad_id, "OAFIID:nautilus_mozilla_content_view:1ee70717-57bf-4079-aae5-922abdd576b1")) {
		return NULL;
	}

	DEBUG_MSG (("+mozilla_make_object\n"));

	object_count++;

	bonobo_object = nautilus_mozilla_content_view_new ();

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy", mozilla_object_destroyed, NULL);

	/* Remove any pending quit-timeout callback */
	if (quit_timeout_id != 0) {
		gtk_timeout_remove (quit_timeout_id);
		quit_timeout_id = 0;
	}

	DEBUG_MSG (("-mozilla_make_object\n"));

	return BONOBO_OBJECT (bonobo_object);
}

extern gboolean test_make_full_uri_from_relative (void);

static gboolean
run_test_cases (void)
{
	return test_make_full_uri_from_relative ();
}

static gboolean
is_good_mozilla_path (const char *path)
{
	char *file_name;
	gboolean good;

	file_name = g_strconcat (path, "/chrome/embed.jar", NULL);
	good = g_file_exists (file_name);
	g_free (file_name);
	return good;
}

static char *
get_mozilla_path (void)
{
	/* Use a set of paths that is similar to the one used by
	 * Galeon. No real harm in searching too many places.
	 */
	guint i;
	const char * const paths[] = {
		PREFIX "/lib/mozilla",
		"/usr/lib/mozilla",
		"/usr/local/mozilla",
		"/opt/mozilla",
		PREFIX "/lib/mozilla-0.8.1" /* lame, but helpful for finding Ximian package */
		"/usr/lib/mozilla-0.8.1" /* lame, but helpful for finding Ximian package */
	};

	for (i = 0; i < EEL_N_ELEMENTS (paths); i++) {
		if (is_good_mozilla_path (paths[i])) {
			return g_strdup (paths[i]);
		}
	}

	return NULL;
}

static void
set_up_MOZILLA_FIVE_HOME (void)
{
	char *mozilla_path;

	if (g_getenv ("MOZILLA_FIVE_HOME") != NULL) {
		return;
	}

	mozilla_path = get_mozilla_path ();
	if (mozilla_path == NULL) {
		g_warning ("Unable to find Mozilla with MOZILLA_FIVE_HOME or looking in standard places.");
		return;
	}
	eel_setenv ("MOZILLA_FIVE_HOME", mozilla_path, TRUE);
	g_free (mozilla_path);
}

int
main (int argc, char *argv[])
{
	BonoboGenericFactory *factory;
	CORBA_ORB orb;
	char *registration_id;
	GError *error_gconf = NULL;
	char *fake_argv[] = { "nautilus-mozilla-content-view", NULL };

	DEBUG_MSG (("nautilus-mozilla-content-view: starting...\n"));

	/* set MOZILLA_FIVE_HOME if it's not already set */
	set_up_MOZILLA_FIVE_HOME();

	if (argc == 2 && 0 == strcmp (argv[1], "--self-test")) {
		gboolean success;

		success = run_test_cases();

		exit (success ? 0 : -1);
	}
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);

	gnome_init ("nautilus-mozilla-content-view", VERSION, 
		    argc, argv); 
	gdk_rgb_init ();
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger (G_LOG_DOMAIN, NULL);
	}

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

	/* Initialize gettext support */
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	gnome_vfs_init ();

	/* We want the XPCOM runtime to stick around longer than
	 * the lifetime of a gtkembedmoz widget.
	 * The corresponding pop_startup is in mozilla_process_delayed_exit
	 * above
	 */
	gtk_moz_embed_push_startup ();

	DEBUG_MSG (("nautilus-mozilla-content-view: OAF registration complete.\n"));

	do {
		bonobo_main ();
	} while (object_count > 0);

        gnome_vfs_shutdown ();

	return 0;
}
