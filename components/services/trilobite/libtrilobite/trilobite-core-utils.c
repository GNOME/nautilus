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
#include <sys/time.h>
#include <gnome.h>

#ifndef TRILOBITE_SLIM 
#include <libgnomevfs/gnome-vfs.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#else /* TRILOBITE_SLIM */
#include <ghttp.h>
#endif /* TRILOBITE_SLIM */

#include "trilobite-core-utils.h"

#define TRILOBITE_SERVICE_CONFIG_DIR "/etc/trilobite"
#define TRILOBITE_SERVICE_CONFIG_DIR_ENV "TRILOBITE_CONFIG"

#define ROBEY_LIKES_TIMESTAMPS

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
static FILE *saved_logf = NULL;
static int do_debug_log = 0;

/* handler for trapping g_log/g_warning/g_error/g_message stuff, and sending it to
 * a standard logfile.
 */
static void
trilobite_add_log (const char *domain, GLogLevelFlags flags, const char *message, FILE *logf)
{
	char *prefix;
	char *timestamp = NULL;
#ifdef ROBEY_LIKES_TIMESTAMPS
	struct timeval now;
#endif
	
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

#ifdef ROBEY_LIKES_TIMESTAMPS
	gettimeofday (&now, NULL);
	timestamp = g_malloc (40);
	strftime (timestamp, 40, "%d-%b %H:%M:%S", localtime ((time_t *)&now.tv_sec));
	sprintf (timestamp + strlen (timestamp), ".%02ld ", now.tv_usec/10000L);
#endif

	fprintf (logf, "%s%s %s\n", timestamp != NULL ? timestamp : "", prefix, message);
	fflush (logf);
}

static void
trilobite_close_log (void)
{
	if (saved_logf != NULL) {
		fclose (saved_logf);
	}
}
#endif	/* TRILOBITE_SLIM */


#ifndef TRILOBITE_SLIM
static GnomeVFSHandle *
trilobite_open_uri (const char *uri_text)
{
	GnomeVFSResult err;
	GnomeVFSURI *uri;
	GnomeVFSHandle *handle = NULL;

	if (! gnome_vfs_initialized ()) {
		setenv ("GNOME_VFS_HTTP_USER_AGENT", trilobite_get_useragent_string (FALSE, NULL), 1);

		if (! gnome_vfs_init ()) {
			g_warning ("cannot initialize gnome-vfs!");
			return NULL;
		}
	}

	uri = gnome_vfs_uri_new (uri_text);
	if (uri == NULL) {
		trilobite_debug ("fetch-uri: invalid uri");
		return NULL;
	}

	err = gnome_vfs_open_uri (&handle, uri, GNOME_VFS_OPEN_READ);
	if (err != GNOME_VFS_OK) {

		trilobite_debug ("fetch-uri on '%s': open failed: %s", 
				 uri_text, 
				 gnome_vfs_result_to_string (err));
		handle = NULL;
	}

	gnome_vfs_uri_unref (uri);
	return handle;
}
#endif /* TRILOBITE_SLIM */

#ifndef TRILOBITE_SLIM
/* fetch a file from an url, using gnome-vfs
 * (using gnome-vfs allows urls of the type "eazel-auth:/etc" to work)
 * generally this will be used to fetch XML files.
 * on success, the body will be null-terminated (this helps work around bugs in libxml,
 * and also makes it easy to manipulate a small body using string operations).
 */
gboolean
trilobite_fetch_uri (const char *uri_text, char **body, int *length)
{
	GnomeVFSResult err;
	GnomeVFSHandle *handle;
	int buffer_size;
	GnomeVFSFileSize bytes;

	handle = trilobite_open_uri (uri_text);
	if (handle == NULL) {
		return FALSE;
	}

	/* start the buffer at a reasonable size */
	buffer_size = 4096;
	*body = g_malloc (buffer_size);
	*length = 0;

	while (1) {
		/* i think this is probably pretty loser: */
		g_main_iteration (FALSE);
		err = gnome_vfs_read (handle, (*body) + (*length), buffer_size - (*length), &bytes);
		if ((bytes == 0) || (err != GNOME_VFS_OK)) {
			break;
		}
		*length += bytes;
		if (*length >= buffer_size - 64) {
			/* expando time! */
			buffer_size *= 2;
			*body = g_realloc (*body, buffer_size);
		}
	}

	/* EOF is now an "error" :) */
	if ((err != GNOME_VFS_OK) && (err != GNOME_VFS_ERROR_EOF)) {
		g_free (*body);
		*body = NULL;
		goto fail;
	}

	(*body)[*length] = 0;
	gnome_vfs_close (handle);
	return TRUE;

fail:
	trilobite_debug ("fetch-uri on %s: %s (%d)", uri_text, gnome_vfs_result_to_string (err), err);
	gnome_vfs_close (handle);
	return FALSE;
}
#endif /* TRILOBITE_SLIM */

#ifndef TRILOBITE_SLIM
gboolean
trilobite_fetch_uri_to_file (const char *uri_text, const char *filename)
{
	GnomeVFSResult err;
	GnomeVFSHandle *handle;
	FILE *file;
	char buffer[1024];
	GnomeVFSFileSize bytes;

	file = fopen (filename, "w");
	if (file == NULL) {
		return FALSE;
	}

	handle = trilobite_open_uri (uri_text);
	if (handle == NULL) {
		fclose (file);
		return FALSE;
	}

	while (1) {
		g_main_iteration (FALSE);
		err = gnome_vfs_read (handle, buffer, sizeof(buffer), &bytes);
		if ((bytes == 0) || (err != GNOME_VFS_OK)) {
			break;
		}
		fwrite (buffer, bytes, 1, file);
	}

	gnome_vfs_close (handle);
	fclose (file);

	return (err == GNOME_VFS_OK);
}
#endif /* TRILOBITE_SLIM */

#ifdef TRILOBITE_SLIM
gboolean trilobite_fetch_uri (const char *uri_text, 
			      char **body, 
			      int *length)
{
	char *uri = NULL;
        ghttp_request* request;
        ghttp_status status;
	gboolean result = TRUE;

	g_assert (body!=NULL);
	g_assert (uri_text != NULL);
	g_assert (length != NULL);

	uri = g_strdup (uri_text);
        request = NULL;
        (*length) = -1;
        (*body) = NULL;

        if ((request = ghttp_request_new())==NULL) {
                g_warning (_("Could not create an http request !"));
                result = FALSE;
        } 

        if (result && (ghttp_set_uri (request, uri) != 0)) {
                g_warning (_("Invalid uri !"));
                result = FALSE;
        }

	if (result) {
		ghttp_set_header (request, http_hdr_Connection, "close");
		ghttp_set_header (request, http_hdr_User_Agent, trilobite_get_useragent_string (FALSE, NULL));
	}

        if (result && (ghttp_prepare (request) != 0)) {
                g_warning (_("Could not prepare http request !"));
                result = FALSE;
        }

        if (result && ghttp_set_sync (request, ghttp_async)) {
                g_warning (_("Couldn't get async mode "));
                result = FALSE;
        }

        while (result && (status = ghttp_process (request)) == ghttp_not_done) {
		/*                ghttp_current_status curStat = ghttp_get_status (request); */
		g_main_iteration (FALSE);
        }

        if (result && (ghttp_status_code (request) != 200)) {
                g_warning (_("HTTP error: %d %s"), ghttp_status_code (request),
			   ghttp_reason_phrase (request));
                result = FALSE;
        }
	if (result && (ghttp_status_code (request) != 404)) {
		(*length) = ghttp_get_body_len (request);
		(*body) = g_new0 (char, *length + 1);
		memcpy (*body, ghttp_get_body (request), *length);
		(*body)[*length] = 0;
	} else {
		result = FALSE;
	}

        if (request) {
                ghttp_request_destroy (request);
        }
	
	g_free (uri);

	return result;
}
#endif /* TRILOBITE_SLIM */

#ifdef TRILOBITE_SLIM
gboolean trilobite_fetch_uri_to_file (const char *uri_text, 
				      const char *filename)
{
	char *body = NULL;
	int length;
	gboolean result = FALSE;

	result =  trilobite_fetch_uri (uri_text, &body, &length);
	if (result) {
		FILE* file;
		file = fopen (filename, "wb");
		if (file == NULL) {
			g_warning (_("Could not open target file %s"),filename);
			result = FALSE;
		} else {
			fwrite (body, length, 1, file);
		}
	} 

	return result;	
}
#endif /* TRILOBITE_SLIM */

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
		int argc, char **argv, GData *options)
{
	CORBA_ORB orb;
	FILE *logf;
	char *real_log_filename;

	/* for future reference:
	 * possible to avoid gtk_init (which requires X) by using gtk_type_init(), gtk_signal_init()
	 * according to george.
	 * gtk_type_init ();
	 * gnomelib_init ("trilobite-eazel-install-service-factory", "0.1");
	 * gnomelib_register_popt_table (oaf_popt_options, "Trilobite-Eazel-Install-Server");
	 * orb = oaf_init (argc, argv);
	 * gnomelib_parse_args (argc, argv, 0);
	 */
	gnome_init_with_popt_table (service_name, version_name, argc, argv, oaf_popt_options, 0, NULL);
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
			g_log_set_handler (service_name, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
					   G_LOG_LEVEL_ERROR | G_LOG_LEVEL_DEBUG,
					   (GLogFunc)trilobite_add_log, logf);
			/* send libtrilobite messages there, too */
			g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING |
					   G_LOG_LEVEL_ERROR | G_LOG_LEVEL_DEBUG,
					   (GLogFunc)trilobite_add_log, logf);
		} else {
			g_warning (_("Can't write logfile %s -- using default log handler"), real_log_filename);
		}
		g_free (real_log_filename);
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

void
trilobite_set_debug_mode (gboolean debug_mode)
{
	do_debug_log = (debug_mode ? 1 : 0);
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
	char *string;

	if (! overwrite && g_getenv (name) != NULL) {
		return FALSE;
	}

	/* This results in a leak when you overwrite existing
	 * settings. It would be fairly easy to fix this by keeping
	 * our own parallel array or hash table.
	 */
	string = g_strconcat (name, "=", value, NULL);
	return putenv (string);
#endif
}

void
trilobite_debug (const gchar *format, ...)
{
	va_list args;
	va_start (args, format);
	g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
	va_end (args);
}

