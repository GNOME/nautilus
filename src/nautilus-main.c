/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@eazel.com>,
 *          John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>
#include "nautilus-main.h"

#include "nautilus-application.h"
#include "nautilus-window.h"
#include "nautilus-self-check-functions.h"
#include <bonobo/bonobo-main.h>
#include <dlfcn.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-lib-self-check-functions.h>
#include <libnautilus-extensions/nautilus-self-checks.h>
#include <liboaf/liboaf.h>
#include <gtk/gtkmain.h>
#include <popt.h>
#include <stdlib.h>

/* Keeps track of everyone who wants the main event loop kept active */
static GSList *nautilus_main_event_loop_registrants;

static gboolean
nautilus_main_is_event_loop_needed (void)
{
	return nautilus_main_event_loop_registrants != NULL;
}

static void
nautilus_main_event_loop_unregister (GtkObject* object)
{
	g_assert (g_slist_find (nautilus_main_event_loop_registrants, object) != NULL);
	nautilus_main_event_loop_registrants = g_slist_remove (nautilus_main_event_loop_registrants, object);
	if (!nautilus_main_is_event_loop_needed () && gtk_main_level () > 0) {
		gtk_main_quit ();
	}
}

void
nautilus_main_event_loop_register (GtkObject* object)
{
	gtk_signal_connect (object, "destroy", nautilus_main_event_loop_unregister, NULL);
	nautilus_main_event_loop_registrants = g_slist_prepend (nautilus_main_event_loop_registrants, object);
}

gboolean
nautilus_main_is_event_loop_mainstay (GtkObject* object)
{
	return g_slist_length (nautilus_main_event_loop_registrants) == 1 && nautilus_main_event_loop_registrants->data == object;
}

void
nautilus_main_event_loop_quit (void)
{
	while (nautilus_main_event_loop_registrants != NULL) {
		gtk_object_destroy (nautilus_main_event_loop_registrants->data);
	}
}

int
main (int argc, char *argv[])
{
	gboolean kill_shell;
	gboolean restart_shell;
	gboolean stop_desktop;
	gboolean start_desktop;
	gboolean perform_self_check;
	
	poptContext popt_context;
	const char **args;
	
	CORBA_ORB orb;
	NautilusApplication *application;
	
	struct poptOption options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		{ "check", '\0', POPT_ARG_NONE, &perform_self_check, 0, N_("Perform high-speed self-check tests."), NULL },
#endif
		{ "quit", '\0', POPT_ARG_NONE, &kill_shell, 0, N_("Quit Nautilus."), NULL },
		{ "restart", '\0', POPT_ARG_NONE, &restart_shell, 0, N_("Restart Nautilus."), NULL },
		{ "stop-desktop", '\0', POPT_ARG_NONE, &stop_desktop, 0, N_("Don't draw background and icons on desktop."), NULL },
		{ "start-desktop", '\0', POPT_ARG_NONE, &start_desktop, 0, N_("Draw background and icons on desktop."), NULL },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", NULL);
	}
	
	/* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif
	
	/* Initialize the services that we use. */
	kill_shell		= FALSE;
	restart_shell		= FALSE;
	stop_desktop		= FALSE;
	start_desktop		= FALSE;
	perform_self_check	= FALSE;

        gnome_init_with_popt_table ("nautilus", VERSION,
				    argc, argv, options, 0,
				    &popt_context);
				    
	g_thread_init (NULL);
	orb = oaf_init (argc, argv);
	gnome_vfs_init ();
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
	bonobo_activate (); /* do now since we need it before main loop */

	/* Do either the self-check or the real work. */
	if (perform_self_check) {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		/* Run the checks for nautilus and libnautilus. */
		nautilus_run_self_checks ();
		nautilus_run_lib_self_checks ();
		nautilus_exit_if_self_checks_failed ();
#endif
	} else {
		/* Run the nautilus application. */

		args = poptGetArgs (popt_context);

		if (kill_shell && args != NULL) {
			fprintf(stderr, _("nautilus: --quit cannot be used with URIs.\n"));
		} else if (restart_shell && args != NULL) {
			fprintf(stderr, _("nautilus: --restart cannot be used with URIs.\n"));
		} else if (kill_shell && start_desktop) {
			fprintf(stderr, _("nautilus: --quit and --start-desktop cannot be used together.\n"));
		} else if (restart_shell && start_desktop) {
			fprintf(stderr, _("nautilus: --restart and --start-desktop cannot be used together.\n"));
		} else if (stop_desktop && start_desktop) {
			fprintf(stderr, _("nautiluls: --stop-desktop and --start-desktop cannot be used together.\n"));
		} else {
			application = nautilus_application_new ();
			nautilus_application_startup (application,
						      kill_shell,
						      restart_shell,
						      stop_desktop,
						      start_desktop,
						      args);
			if (nautilus_main_is_event_loop_needed ()) {
				bonobo_main ();
			}
			bonobo_object_unref (BONOBO_OBJECT (application));
		}
		poptFreeContext(popt_context);
	}

	gnome_vfs_shutdown ();

	/* if told to restart, exec() myself again */
	if (getenv ("_NAUTILUS_RESTART")) {
		char *my_path = argv[0];
		char **argv_copy;
		int i;

		unsetenv ("_NAUTILUS_RESTART");

		/* might eventually want to copy all the parameters from argv into the new exec.
		 * for now, though, that would just interfere with the re-creation of windows
		 * (whose info is stored in gconf).
		 */
		argv_copy = g_new0 (char *, 2);
		argv_copy[0] = argv[0];
		argv_copy[1] = NULL;
		i = i;

		execvp(my_path, argv_copy);
	}

	return EXIT_SUCCESS;
}
