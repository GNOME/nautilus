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
#include <signal.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-self-checks.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtksignal.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>
#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>
#include <libnautilus-private/nautilus-debug-log.h>
#include <libnautilus-private/nautilus-directory-metafile.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-lib-self-check-functions.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libxml/parser.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

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

	/* We can be called even outside the main loop,
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

static gboolean
initial_event_loop_needed (gpointer data)
{
	if (!is_event_loop_needed ()) {
		eel_gtk_main_quit_all ();
	}
	return FALSE;
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
nautilus_main_event_loop_quit (gboolean explicit)
{
	if (explicit) {
		/* Explicit --quit, make sure we don't restart */
		gnome_client_set_restart_style (gnome_master_client (),
						GNOME_RESTART_IF_RUNNING);
	}
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

static void
dump_debug_log (void)
{
	char *filename;

	filename = g_build_filename (g_get_home_dir (), "nautilus-debug-log.txt", NULL);
	nautilus_debug_log_dump (filename, NULL); /* NULL GError */
	g_free (filename);
}

static int debug_log_pipes[2];

static gboolean
debug_log_io_cb (GIOChannel *io, GIOCondition condition, gpointer data)
{
	char a;

	while (read (debug_log_pipes[0], &a, 1) != 1)
		;

	nautilus_debug_log (TRUE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "user requested dump of debug log");

	dump_debug_log ();
	return FALSE;
}

static void
sigusr1_handler (int sig)
{
	while (write (debug_log_pipes[1], "a", 1) != 1)
		;
}

/* This is totally broken as we're using non-signal safe
 * calls in sigfatal_handler. Disable by default. */
#ifdef USE_SEGV_HANDLER

/* sigaction structures for the old handlers of these signals */
static struct sigaction old_segv_sa;
static struct sigaction old_abrt_sa;
static struct sigaction old_trap_sa;
static struct sigaction old_fpe_sa;
static struct sigaction old_bus_sa;

static void
sigfatal_handler (int sig)
{
	void (* func) (int);

	/* FIXME: is this totally busted?  We do malloc() inside these functions,
	 * and yet we are inside a signal handler...
	 */
	nautilus_debug_log (TRUE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "debug log dumped due to signal %d", sig);
	dump_debug_log ();

	switch (sig) {
	case SIGSEGV:
		func = old_segv_sa.sa_handler;
		break;

	case SIGABRT:
		func = old_abrt_sa.sa_handler;
		break;

	case SIGTRAP:
		func = old_trap_sa.sa_handler;
		break;

	case SIGFPE:
		func = old_fpe_sa.sa_handler;
		break;

	case SIGBUS:
		func = old_bus_sa.sa_handler;
		break;

	default:
		func = NULL;
		break;
	}

	/* this scares me */
	if (func != NULL && func != SIG_IGN && func != SIG_DFL)
		(* func) (sig);
}
#endif

static void
setup_debug_log_signals (void)
{
	struct sigaction sa;
	GIOChannel *io;

	if (pipe (debug_log_pipes) == -1)
		g_error ("Could not create pipe() for debug log");

	io = g_io_channel_unix_new (debug_log_pipes[0]);
	g_io_add_watch (io, G_IO_IN, debug_log_io_cb, NULL);

	sa.sa_handler = sigusr1_handler;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction (SIGUSR1, &sa, NULL);

	/* This is totally broken as we're using non-signal safe
	 * calls in sigfatal_handler. Disable by default. */
#ifdef USE_SEGV_HANDLER
	sa.sa_handler = sigfatal_handler;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGSEGV, &sa, &old_segv_sa);
	sigaction(SIGABRT, &sa, &old_abrt_sa);
	sigaction(SIGTRAP, &sa, &old_trap_sa);
	sigaction(SIGFPE,  &sa, &old_fpe_sa);
	sigaction(SIGBUS,  &sa, &old_bus_sa);
#endif
}

static GLogFunc default_log_handler;

static void
log_override_cb (const gchar   *log_domain,
		 GLogLevelFlags log_level,
		 const gchar   *message,
		 gpointer       user_data)
{
	gboolean is_debug;
	gboolean is_milestone;

	is_debug = ((log_level & G_LOG_LEVEL_DEBUG) != 0);
	is_milestone = !is_debug;

	nautilus_debug_log (is_milestone, NAUTILUS_DEBUG_LOG_DOMAIN_GLOG, "%s", message);

	if (!is_debug)
		(* default_log_handler) (log_domain, log_level, message, user_data);
}

static void
setup_debug_log_glog (void)
{
	default_log_handler = g_log_set_default_handler (log_override_cb, NULL);
}

static void
setup_debug_log (void)
{
	char *config_filename;

	config_filename = g_build_filename (g_get_home_dir (), "nautilus-debug-log.conf", NULL);
	nautilus_debug_log_load_configuration (config_filename, NULL); /* NULL GError */
	g_free (config_filename);

	setup_debug_log_signals ();
	setup_debug_log_glog ();
}

int
main (int argc, char *argv[])
{
	gboolean kill_shell;
	gboolean restart_shell;
	gboolean no_default_window;
	gboolean browser_window;
	gboolean no_desktop;
	gboolean autostart_mode;
	const char *startup_id, *autostart_id;
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

	g_thread_init (NULL);

	setlocale (LC_ALL, "");

	/* This will be done by gtk+ later, but for now, force it to GNOME */
	g_desktop_app_info_set_desktop_env ("GNOME");

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
		g_unsetenv ("DESKTOP_STARTUP_ID");
	}

	autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
	if (autostart_id != NULL && *autostart_id != '\0') {
		autostart_mode = TRUE;
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

#ifdef HAVE_EXEMPI
	xmp_init();
#endif

	program = gnome_program_init ("nautilus", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("Nautilus"),
				      NULL);

	/* We do this after gnome_program_init(), since that function sets up
	 * its own handler for SIGSEGV and others --- we want to chain to those
	 * handlers.
	 */
	setup_debug_log ();

	if (session_to_load != NULL) {
		no_default_window = TRUE;
	}

	/* If in autostart mode (aka started by gnome-session), we need to ensure 
         * nautilus starts with the correct options.
         */
	if (autostart_mode) {
		no_default_window = TRUE;
		no_desktop = FALSE;
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
	gtk_window_set_default_icon_name (NAUTILUS_ICON_FOLDER);
	
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
			 browser_window,
			 startup_id_copy,
			 geometry,
			 session_to_load,
			 remaining);
		g_free (startup_id_copy);

		/* The application startup does things in an idle, so
		   we need to check whether the main loop is needed in an idle
		*/
		g_idle_add (initial_event_loop_needed, NULL);
		gtk_main ();
	}

	nautilus_icon_info_clear_caches ();
	
	if (application != NULL) {
		bonobo_object_unref (application);
	}

 	eel_debug_shut_down ();
	
	/* If told to restart, exec() myself again. This is used when
	 * the program is told to restart with CORBA, for example when
	 * an update takes place.
	 */

	if (g_getenv ("_NAUTILUS_RESTART_SESSION_FILENAME") != NULL) {
		argv_copy = g_new0 (char *, 4);
		argv_copy[0] = g_strdup (argv[0]);
		argv_copy[1] = g_strdup ("--load-session");
		argv_copy[2] = g_strdup (g_getenv ("_NAUTILUS_RESTART_SESSION_FILENAME"));

		g_unsetenv ("_NAUTILUS_RESTART_SESSION_FILENAME");

		execvp (argv[0], argv_copy);

		g_strfreev (argv_copy);
	}

	g_object_unref (G_OBJECT (program));

	return EXIT_SUCCESS;
}
