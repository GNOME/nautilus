/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *	    Robey Pointer <robey@eazel.com>
 */

/*
 * libtrilobite - Useful functions shared between all services.  This
 * includes things like xml parsing, logging, error control, and others.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gnome.h>

#include "trilobite-core-utils.h"
#include "trilobite-core-messaging.h"

#ifndef TRILOBITE_SLIM 
#include <liboaf/liboaf.h>
#include <bonobo.h>
#endif /* TRILOBITE_SLIM */


#define TRILOBITE_SERVICE_CONFIG_DIR "/etc/trilobite"
#define TRILOBITE_SERVICE_CONFIG_DIR_ENV "TRILOBITE_CONFIG"

#ifdef OPEN_MAX
#define LAST_FD OPEN_MAX
#else
#define LAST_FD 1024
#endif

/* better/safer replacement for popen (which doesn't exist on all platforms, anyway).
 * forks and executes a specific program, with pipes carrying stdin/stdout/stderr back
 * to you.  this way, you can fork off a subprocess and control its environment. and
 * it doesn't use system() like most evil popen implementations.
 * for the future, it'd be nice to be able to clear out the subprocess's env vars.
 */
int
trilobite_pexec (const char *path, char * const argv[], int *stdin_fd, int *stdout_fd, int *stderr_fd)
{
	pid_t child;
	int pipe_in[2], pipe_out[2], pipe_err[2];
	int i;

	pipe_in[0] = pipe_in[1] = pipe_out[0] = pipe_out[1] = pipe_err[0] = pipe_err[1] = -1;
	if ((pipe (pipe_in) != 0) || (pipe (pipe_out) != 0) || (pipe (pipe_err) != 0)) {
		goto close_and_give_up;
	}
	child = fork ();
	if (child < 0) {
		goto close_and_give_up;
	}

	if (child == 0) {
#if 0
		child = fork ();
		if (child != 0) {
			exit (0);
		}

		/* keep child processes from trying to write to the tty */
		setsid ();
		setpgid (0, 0);
#endif

		/* make stdin/stdout/stderr use the pipes */
		if (stdin_fd) {
			dup2 (pipe_in[0], 0);
		}
		if (stdout_fd) {
			dup2 (pipe_out[1], 1);
		}
		if (stderr_fd) {
			dup2 (pipe_err[1], 2);
		}
		/* close all open fd's */
		for (i = 3; i < LAST_FD; i++) {
			close(i);
		}

		/* FIXME bugzilla.eazel.com 2589: might we want to specify our own environment here? */
		execv (path, argv);

		/* if we get here, then somehow, exec failed */
		exit (-1);
	}

	/* copy out all the in/out/err fd's */
	close (pipe_in[0]);
	close (pipe_out[1]);
	close (pipe_err[1]);
	if (stdin_fd) {
		*stdin_fd = pipe_in[1];
	} else {
		close (pipe_in[1]);
	}
	if (stdout_fd) {
		*stdout_fd = pipe_out[0];
	} else {
		close (pipe_out[0]);
	}
	if (stderr_fd) {
		*stderr_fd = pipe_err[0];
	} else {
		close (pipe_err[0]);
	}

	return (int)child;

close_and_give_up:
	close (pipe_in[0]);
	close (pipe_in[1]);
	close (pipe_out[0]);
	close (pipe_out[1]);
	close (pipe_err[0]);
	close (pipe_err[1]);

	return 0;
}


#ifndef TRILOBITE_SLIM

/* trilobite_init -- does all the init stuff 
 * FIXME bugzilla.eazel.com 1656:
 * for now, this requires init_gnome, and thus a running X server.
 * initializes OAF and bonobo, too.
 *
 * service_name should be your G_LOG_DOMAIN, if you set one, because of the way logging works
 * 
 * options is a way to add extra parameters in the future (can be NULL for now).
 *
 * returns FALSE if init fails.
 */
gboolean
trilobite_init (const char *service_name, const char *version_name, const char *log_filename,
		int argc, char **argv)
{
	CORBA_ORB orb;
	FILE *logf;
	char *real_log_filename;

#ifdef TRILOBITE_USE_X
	gnome_init_with_popt_table (service_name, version_name, argc, argv, oaf_popt_options, 0, NULL);
#else
	gtk_type_init ();
	gtk_signal_init ();
	gnomelib_init (service_name, version_name);
	gnomelib_register_popt_table (oaf_popt_options, service_name);
	gnomelib_parse_args (argc, argv, 0);
#endif
	orb = oaf_init (argc, argv);

	if (!bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_error (_("Could not initialize Bonobo"));
		goto fail;
	}

	if (log_filename != NULL) {
		if ((log_filename[0] == '~') && (log_filename[1] == '/')) {
			real_log_filename = g_strdup_printf ("%s%s", g_get_home_dir (), log_filename+1);
		} else {
			real_log_filename = g_strdup (log_filename);
		}

		logf = fopen (real_log_filename, "wt");
		if (logf != NULL) {
			trilobite_set_log_handler (logf, service_name);
		} else {
			g_warning (_("Can't write logfile %s -- using default log handler"), real_log_filename);
		}
		g_free (real_log_filename);
	}

	return TRUE;

fail:
	return FALSE;
}
#endif /* TRILOBITE_SLIM */

const char *
trilobite_get_useragent_string (gboolean version, char *suffix)
{
	static char *result = NULL;

	g_free (result);
	result = g_strdup_printf ("Trilobite/%s%s%s", 
				  version ? "/" TRILOBITE_VERSION : suffix ? "/" : "" , 
				  suffix ? "/" : "", 
				  suffix ? suffix : "");
	return result;
}

const char *
trilobite_get_config_dir_string ()
{
	static const char *res = NULL;

	if (res!=NULL) {
		return res;
	}

	if (getenv (TRILOBITE_SERVICE_CONFIG_DIR_ENV)) {
		res = g_strdup (getenv (TRILOBITE_SERVICE_CONFIG_DIR_ENV));
	} else {
		res = g_strdup (TRILOBITE_SERVICE_CONFIG_DIR);
	}
	
	return res;
}

/* copied from libnautilus-extensions */
gboolean
trilobite_setenv (const char *name, const char *value, gboolean overwrite)
{
#if defined (HAVE_SETENV)
	return (setenv (name, value, overwrite) == 0);
#else
	char *string = NULL;

	if (!overwrite && getenv (name) != NULL) {
		return FALSE;
	}

	/* This results in a leak when you overwrite existing
	 * settings. It would be fairly easy to fix this by keeping
	 * our own parallel array or hash table.
	 * FIXME: bugzilla.eazel.com 2900
	 */
	string = g_strconcat (name, "=", value, NULL);
	return putenv (string);
#endif
}
