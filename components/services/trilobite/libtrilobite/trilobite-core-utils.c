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
 *
 */

/*
 * libtrilobite - Useful functions shared between all services.  This
 * includes things like xml parsing, logging, error control, and others.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include "trilobite-core-utils.h"

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

		/* FIXME: might we want to specify our own environment here? */
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

	return 0;

close_and_give_up:
	close (pipe_in[0]);
	close (pipe_in[1]);
	close (pipe_out[0]);
	close (pipe_out[1]);
	close (pipe_err[0]);
	close (pipe_err[1]);

	return -1;
}


static FILE *saved_logf = NULL;
static int do_debug_log = 0;

/* handler for trapping g_log/g_warning/g_error/g_message stuff, and sending it to
 * a standard logfile.
 */
static void
trilobite_add_log (const char *domain, GLogLevelFlags flags, const char *message, FILE *logf)
{
	char *prefix;
	g_assert (logf != NULL);

	if (flags & G_LOG_LEVEL_DEBUG) {
		if (do_debug_log) {
			prefix = "/// debug:";
		} else {
			return;
		}
	} else if (flags & G_LOG_LEVEL_MESSAGE) {
		prefix = "---";
	} else if (flags & G_LOG_LEVEL_WARNING) {
		prefix = "*** warning:";
	} else if (flags & G_LOG_LEVEL_ERROR) {
		prefix = "!!! ERROR:";
	} else {
		prefix = "???";
	}

	fprintf (logf, "%s %s\n", prefix, message);
	fflush (logf);
}

static void
trilobite_close_log (void)
{
	if (saved_logf != NULL) {
		fclose (saved_logf);
	}
}


/* trilobite_init -- does all the init stuff 
 * for now, this requires init_gnome, and thus a running X server: FIXME robey (1656)
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
		int argc, char **argv, GData *options)
{
	CORBA_ORB orb;
	FILE *logf;

	gnome_init_with_popt_table (service_name, version_name, argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error (_("Could not initialize Bonobo"));
		goto fail;
	}

	if (log_filename != NULL) {
		logf = fopen (log_filename, "wt");
		if (logf != NULL) {
			g_log_set_handler (service_name, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
					   G_LOG_LEVEL_ERROR | G_LOG_LEVEL_DEBUG,
					   (GLogFunc)trilobite_add_log, logf);
			/* send libtrilobite messages there, too */
			g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
					   G_LOG_LEVEL_ERROR | G_LOG_LEVEL_DEBUG,
					   (GLogFunc)trilobite_add_log, logf);
		} else {
			g_warning (_("Can't write logfile %s -- using default log handler"), log_filename);
		}
	}

	g_atexit (trilobite_close_log);

	if (g_datalist_get_data (&options, "debug") != NULL) {
		/* debug mode on */
		do_debug_log = 1;
	}

	return TRUE;

fail:
	return FALSE;
}
