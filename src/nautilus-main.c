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

#include "config.h"
#include "nautilus.h"
#include "nautilus-self-check-functions.h"
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-lib-self-check-functions.h>
#include <libnautilus-extensions/nautilus-self-checks.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <nautilus-widgets/nautilus-widgets-self-check-functions.h>
#include <nautilus-widgets/nautilus-preferences.h>
#include <libgnomevfs/gnome-vfs-init.h>

int
main(int argc, char *argv[])
{
	poptContext ctx;
	CORBA_Environment ev;
	CORBA_ORB orb;
	gboolean preferences_result;
#if !defined (NAUTILUS_OMIT_SELF_CHECK)
	gboolean check = FALSE;
#endif
	const char **args;
	NautilusApp *app;

	struct poptOption options[] = {
#if !defined (NAUTILUS_OMIT_SELF_CHECK)
		{ "check", '\0', POPT_ARG_NONE, &check, 0, N_("Perform high-speed self-check tests."), NULL },
#endif
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};
	
	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (getenv ("NAUTILUS_DEBUG") != NULL) {
		nautilus_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", NULL);
	}
	
	/* Initialize the services that we use. */
	CORBA_exception_init (&ev);

        gnome_init_with_popt_table ("nautilus", VERSION, 
				   argc, argv,
				   options, 0, &ctx); 
	orb = oaf_init (argc, argv);

	preferences_result = nautilus_preferences_initialize (argc, argv);

	/* FIXME bugzilla.eazel.com 672: 
	 * Need better error reporting if this fails.  BUT, is it too
	 * early to post a dialog here ? */
	g_assert (preferences_result);
	
	bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
	g_thread_init (NULL);
	gnome_vfs_init ();
	nautilus_undo_manager_initialize_global_manager ();
	
	args = poptGetArgs (ctx);

#if !defined (NAUTILUS_OMIT_SELF_CHECK)
	if (check) {
		/* Run the checks for nautilus and libnautilus. */
		nautilus_run_self_checks ();
		nautilus_run_lib_self_checks ();
		nautilus_widgets_run_self_checks ();
		nautilus_exit_if_self_checks_failed ();
	} else {
#endif

		/* Run the nautilus application. */
		app = NAUTILUS_APP (nautilus_app_new ());
		nautilus_app_startup (app, args ? args[0] : NULL);
		bonobo_main();
		bonobo_object_unref (BONOBO_OBJECT (app));

#if !defined (NAUTILUS_OMIT_SELF_CHECK)
	}
#endif

	return EXIT_SUCCESS;
}

