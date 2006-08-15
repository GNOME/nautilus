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
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 *
 */

/* nautilus-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>
#include "nautilus-main.h"

#include "nautilus-application.h"
#include "nautilus-self-check-functions.h"
#include "nautilus-window.h"
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-main.h>
#include <dlfcn.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-self-checks.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtksignal.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libnautilus-private/nautilus-directory-metafile.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-lib-self-check-functions.h>
#include <libxml/parser.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Keeps track of everyone who wants the main event loop kept active */
static GSList *event_loop_registrants;

static gboolean
is_event_loop_needed (void)
{
	return event_loop_registrants != NULL;
}

static int
quit_if_in_main_loop (gpointer callback_data)
{
	guint level;

	g_assert (callback_data == NULL);

	level = gtk_main_level ();

	/* We can be called even outside the main loop by gnome_vfs_shutdown,
	 * so check that we are in a loop before calling quit.
	 */
	if (level != 0) {
		gtk_main_quit ();
	}

	/* We need to be called again if we quit a nested loop. */
	return level > 1;
}

static void
eel_gtk_main_quit_all (void)
{
	/* Calling gtk_main_quit directly only kills the current/top event loop.
	 * This idler will be run by the current event loop, killing it, and then
	 * by the next event loop, ...
	 */
	g_idle_add (quit_if_in_main_loop, NULL);
}

static void
event_loop_unregister (GtkObject *object)
{
	event_loop_registrants = g_slist_remove (event_loop_registrants, object);
	if (!is_event_loop_needed ()) {
		eel_gtk_main_quit_all ();
	}
}

void
nautilus_main_event_loop_register (GtkObject *object)
{
	g_signal_connect (object, "destroy", G_CALLBACK (event_loop_unregister), NULL);
	event_loop_registrants = g_slist_prepend (event_loop_registrants, object);
}

gboolean
nautilus_main_is_event_loop_mainstay (GtkObject *object)
{
	return g_slist_length (event_loop_registrants) == 1
		&& event_loop_registrants->data == object;
}

void
nautilus_main_event_loop_quit (void)
{
	while (event_loop_registrants != NULL) {
		gtk_object_destroy (event_loop_registrants->data);
	}
}

/* Copied from libnautilus/nautilus-program-choosing.c; In this case,
 * though, it's really needed because we have no real alternative when
 * no DESKTOP_STARTUP_ID (with its accompanying timestamp) is
 * provided...
 */
static Time
slowly_and_stupidly_obtain_timestamp (Display *xdisplay)
{
	Window xwindow;
	XEvent event;
	
	{
		XSetWindowAttributes attrs;
		Atom atom_name;
		Atom atom_type;
		char* name;
		
		attrs.override_redirect = True;
		attrs.event_mask = PropertyChangeMask | StructureNotifyMask;
		
		xwindow =
			XCreateWindow (xdisplay,
				       RootWindow (xdisplay, 0),
				       -100, -100, 1, 1,
				       0,
				       CopyFromParent,
				       CopyFromParent,
				       (Visual *)CopyFromParent,
				       CWOverrideRedirect | CWEventMask,
				       &attrs);
		
		atom_name = XInternAtom (xdisplay, "WM_NAME", TRUE);
		g_assert (atom_name != None);
		atom_type = XInternAtom (xdisplay, "STRING", TRUE);
		g_assert (atom_type != None);
		
		name = "Fake Window";
		XChangeProperty (xdisplay, 
				 xwindow, atom_name,
				 atom_type,
				 8, PropModeReplace, name, strlen (name));
	}
	
	XWindowEvent (xdisplay,
		      xwindow,
		      PropertyChangeMask,
		      &event);
	
	XDestroyWindow(xdisplay, xwindow);
	
	return event.xproperty.time;
}

int
main (int argc, char *argv[])
{
	gboolean kill_shell;
	gboolean restart_shell;
	gboolean no_default_window;
	gboolean browser_window;
	gboolean no_desktop;
	const char *startup_id;
	char *startup_id_copy;
	char *session_to_load;
	gchar *geometry;
	const gchar **remaining;
	gboolean perform_self_check;
	GOptionContext *context;
	NautilusApplication *application;
	char **argv_copy;
	GnomeProgram *program;
	
	const GOptionEntry options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		{ "check", 'c', 0, G_OPTION_ARG_NONE, &perform_self_check, 
		  N_("Perform a quick set of self-check tests."), NULL },
#endif
		{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
		  N_("Create the initial window with the given geometry."), N_("GEOMETRY") },
		{ "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
		  N_("Only create windows for explicitly specified URIs."), NULL },
		{ "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop,
		  N_("Do not manage the desktop (ignore the preference set in the preferences dialog)."), NULL },
		{ "browser", '\0', 0, G_OPTION_ARG_NONE, &browser_window, 
		  N_("open a browser window."), NULL },
		{ "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
		  N_("Quit Nautilus."), NULL },
		{ "restart", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &restart_shell,
		  N_("Restart Nautilus."), NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining, NULL,  N_("[URI...]") },
		{ "load-session", 'l', 0, G_OPTION_ARG_STRING, &session_to_load,
		  /* Translators: --no-default-window is a nautilus command line parameter, don't modify it. */
		  N_("Load a saved session from the specified file. Implies \"--no-default-window\"."), N_("FILENAME") },

		{ NULL }
	};

	setlocale (LC_ALL, "");

	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
	/* Initialize gettext support */
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	startup_id = g_getenv ("DESKTOP_STARTUP_ID");
	startup_id_copy = NULL;
	if (startup_id != NULL && *startup_id != '\0') {
		/* Clear the DESKTOP_STARTUP_ID, but make sure to copy it first */
		startup_id_copy = g_strdup (startup_id);
		putenv ("DESKTOP_STARTUP_ID=");
	}

	/* we'll do it ourselves due to complicated factory setup */
	gtk_window_set_auto_startup_notification (FALSE);

	/* Get parameters. */
	remaining = NULL;
	geometry = NULL;
	session_to_load = NULL;
	kill_shell = FALSE;
	no_default_window = FALSE;
	no_desktop = FALSE;
	perform_self_check = FALSE;
	restart_shell = FALSE;
	browser_window = FALSE;

	g_set_application_name (_("File Manager"));
	context = g_option_context_new (_("\n\nBrowse the file system with the file manager"));

	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	program = gnome_program_init ("nautilus", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Nautilus"),
				      NULL);

	if (session_to_load != NULL) {
		no_default_window = TRUE;
	}

	/* Do this here so that gdk_display is initialized */
	if (startup_id_copy == NULL) {
		/* Create a fake one containing a timestamp that we can use */
		Time timestamp;
		timestamp = slowly_and_stupidly_obtain_timestamp (gdk_display);
		startup_id_copy = g_strdup_printf ("_TIME%lu",
						   timestamp);
	}

        /* Set default icon for all nautilus windows */
	gtk_window_set_default_icon_name ("gnome-fs-directory");
	
	/* Need to set this to the canonical DISPLAY value, since
	   thats where we're registering per-display components */
	bonobo_activation_set_activation_env_value ("DISPLAY",
						    gdk_display_get_name (gdk_display_get_default()));
	

	if (perform_self_check && remaining != NULL) {
		/* translators: %s is an option (e.g. --check) */
		fprintf (stderr, _("nautilus: %s cannot be used with URIs.\n"),
			"--check");
		return EXIT_FAILURE;
	}
	if (perform_self_check && (kill_shell || restart_shell)) {
		fprintf (stderr, _("nautilus: --check cannot be used with other options.\n"));
		return EXIT_FAILURE;
	}
	if (kill_shell && remaining != NULL) {
		fprintf (stderr, _("nautilus: %s cannot be used with URIs.\n"),
			"--quit");
		return EXIT_FAILURE;
	}
	if (restart_shell && remaining != NULL) {
		fprintf (stderr, _("nautilus: %s cannot be used with URIs.\n"),
			"--restart");
		return EXIT_FAILURE;
	}
	if (geometry != NULL && remaining != NULL && remaining[0] != NULL && remaining[1] != NULL) {
		fprintf (stderr, _("nautilus: --geometry cannot be used with more than one URI.\n"));
		return EXIT_FAILURE;
	}

	/* Initialize the services that we use. */
	LIBXML_TEST_VERSION

	/* Initialize preferences. This is needed so that proper 
	 * defaults are available before any preference peeking 
	 * happens.
	 */
	nautilus_global_preferences_init ();
	if (no_desktop) {
		eel_preferences_set_is_invisible
			(NAUTILUS_PREFERENCES_SHOW_DESKTOP, TRUE);
		eel_preferences_set_is_invisible
			(NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR, TRUE);
	}
	
	bonobo_activate (); /* do now since we need it before main loop */

	application = NULL;
 
	/* Do either the self-check or the real work. */
	if (perform_self_check) {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		/* Run the checks (each twice) for nautilus and libnautilus-private. */

		nautilus_directory_use_self_contained_metafile_factory ();

		nautilus_run_self_checks ();
		nautilus_run_lib_self_checks ();
		eel_exit_if_self_checks_failed ();

		nautilus_run_self_checks ();
		nautilus_run_lib_self_checks ();
		eel_exit_if_self_checks_failed ();
#endif
	} else {
		/* Run the nautilus application. */
		application = nautilus_application_new ();
		nautilus_application_startup
			(application,
			 kill_shell, restart_shell, no_default_window, no_desktop,
			 !(kill_shell || restart_shell),
			 browser_window,
			 startup_id_copy,
			 geometry,
			 session_to_load,
			 remaining);
		g_free (startup_id_copy);

		if (is_event_loop_needed ()) {
			gtk_main ();
		}
	}


	gnome_vfs_shutdown ();

	/* This has to be done after gnome_vfs_shutdown, because shutdown
	 * can call pending completion callbacks which reference application.
	 */
	if (application != NULL) {
		bonobo_object_unref (application);
	}

	eel_debug_shut_down ();

	/* If told to restart, exec() myself again. This is used when
	 * the program is told to restart with CORBA, for example when
	 * an update takes place.
	 */

	if (g_getenv ("_NAUTILUS_RESTART") != NULL) {
		g_unsetenv ("_NAUTILUS_RESTART");
		
		/* Might eventually want to copy all the parameters
		 * from argv into the new exec. For now, though, that
		 * would just interfere with the re-creation of
		 * windows based on the window info stored in gconf,
		 * including whether the desktop was started.
		 */
		argv_copy = g_new0 (char *, 2);
		argv_copy[0] = argv[0];
		
		execvp (argv[0], argv_copy);
	}

	g_object_unref (G_OBJECT (program));

	return EXIT_SUCCESS;
}
