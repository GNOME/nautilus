/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>,
 *           Darin Adler <darin@eazel.com>,
 *           John Sullivan <sullivan@eazel.com>
 *
 */

/* ntl-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "nautilus-application.h"
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
#include <popt.h>
#include <stdlib.h>

/* FIXME: Replace the leak checker calls with weakly exported ones in
 * libnautilus-extensions or in the leak checker library itself.
 */
static void
nautilus_leak_checker_init (const char *path)
{
	void (*real_nautilus_leak_checker_init) (const char *path);

	/* Try to hook up with the leakchecker library. This only
	 * succeeds if we run nautilus with a
	 * LD_PRELOAD=./libleakcheck.so environment variable
	 * setting. If there isn't one, this call is benign.
	 */
	real_nautilus_leak_checker_init = dlsym (RTLD_NEXT, "nautilus_leak_checker_init");
	if (real_nautilus_leak_checker_init != NULL) {
		real_nautilus_leak_checker_init (path);
	}
}

static void 
nautilus_leak_print_leaks (int stack_grouping_depth,
			   int stack_print_depth,
			   int max_count,
			   int sort_by_count)
{
	void (*real_nautilus_leak_print_leaks) (int stack_grouping_depth, 
						int stack_print_depth,
						int max_count,
						int sort_by_count);

	/* Try to hook up with the leak checker library. */
	real_nautilus_leak_print_leaks = dlsym (RTLD_NEXT, "nautilus_leak_print_leaks");
	if (real_nautilus_leak_print_leaks != NULL) {
		real_nautilus_leak_print_leaks (stack_grouping_depth, stack_print_depth,
						max_count, sort_by_count);
	}
}

static void
print_leaks (void)
{
	/* If leak checking, dump all the outstanding allocations just before exiting. */
	nautilus_leak_print_leaks (8, 15, 100, TRUE);
}

int
main (int argc, char *argv[])
{
	gboolean perform_self_check;
	gboolean manage_desktop;
	poptContext popt_context;
	CORBA_ORB orb;
	NautilusApplication *application;
	struct poptOption options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		{ "check", '\0', POPT_ARG_NONE, &perform_self_check, 0, N_("Perform high-speed self-check tests."), NULL },
#endif
		{ "manage-desktop", '\0', POPT_ARG_NONE, &manage_desktop, 0, N_("Draw background and icons on desktop."), NULL },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	/* Set up the leak checker symbol lookup. */
	nautilus_leak_checker_init (argv[0]);
	g_atexit (print_leaks);
	
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
	perform_self_check = FALSE;
	manage_desktop = FALSE;
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
		application = nautilus_application_new ();
		nautilus_application_startup (application,
					      manage_desktop,
					      poptGetArgs (popt_context));
		if (application->windows != NULL) {
			bonobo_main ();
		}
		bonobo_object_unref (BONOBO_OBJECT (application));
	}

	gnome_vfs_shutdown ();

	return EXIT_SUCCESS;
}
