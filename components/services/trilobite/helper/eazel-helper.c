/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * eazel-helper is a standalone binary app that's meant to be used
 * via 'userhelper' by the Eazel services.  You can send it commands
 * over stdin, and it will do certain functions for you (assuming it's
 * running as root).  The command API may change at random, so you
 * should always use TrilobiteRootHelper instead of using eazel-helper
 * directly.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Robey Pointer <robey@eazel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <glib.h>
#include <string.h>

#define RPM_EXEC	"/bin/rpm"

/* paths to search for executables (like RPM) in */
static const char *search_path[] = {
	"/bin",
	"/usr/bin",
	"/usr/local/bin",
	"/sbin",
	"/usr/sbin",
	"/usr/local/sbin",
	NULL
};


static void
chomp (char *buffer)
{
	int x = strlen (buffer);

	while ((x > 0) && ((buffer[x - 1] == '\n') || (buffer[x - 1] == '\r'))) {
		buffer[x - 1] = 0;
		x--;
	}
}

static const char *
find_path_to (const char *filename)
{
	char *path;
	int i;

	for (i = 0; search_path[i]; i++) {
		path = g_strdup_printf ("%s/%s", search_path[i], filename);
		/* i guess g_file_exists() is going away, and not part of glib anyway :( */
		if (access (path, X_OK) == 0) {
			g_free (path);
			return search_path[i];
		}
		g_free (path);
	}

	return NULL;
}

static void
do_command (char *command, int args)
{
	char *filename;
	const char *path;
	char buffer[256];
	char **pargv;
	int i;

	pargv = g_new0 (char *, args + 2);

	path = find_path_to (command);
	if (! path) {
		printf ("* Can't find %s. :(\n", command);
		exit (1);
	}

	filename = g_strdup_printf ("%s/%s", path, command);
	pargv[0] = command;
	for (i = 0; i < args; i++) {
		fgets (buffer, 256, stdin);
		chomp (buffer);
		pargv[i + 1] = g_strdup (buffer);
	}
	pargv[args + 1] = NULL;

	/* we never free any of the args, but it doesn't matter, because
	 * if the exec succeeds, this all suddenly vanishes. :)
	 */
	execv (filename, pargv);

	printf ("* Can't run %s :(\n", command);
	exit (1);
}

int
main (int argc, char **argv)
{
	char buffer[256];
	int args;
	time_t new_time;

	printf ("* OK.\n");
	fflush (stdout);

	/* send stderr to stdout */
	dup2 (1, 2);

	/* get command */
	fgets (buffer, 256, stdin);
	if (feof (stdin)) {
		/* give up */
		exit (1);
	}
	chomp (buffer);

	/* rpm <# of parameters> */
	/* (followed by N lines of parameters) */
	if (g_strncasecmp (buffer, "rpm ", 4) == 0) {
		args = atoi (buffer + 4);
		do_command ("rpm", args);
	}

	/* set-time <time_t> */
	if (g_strncasecmp (buffer, "set-time ", 9) == 0) {
		new_time = strtoul (buffer + 9, NULL, 0);
		if (stime (&new_time) != 0) {
			printf ("X failed: %d\n", errno);
		} else {
			printf ("* done.\n");
		}

		exit (0);
	}

	/* ls <# of parameters> */
	if (g_strncasecmp (buffer, "ls ", 3) == 0) {
		args = atoi (buffer + 3);
		do_command ("ls", args);
	}

	printf ("* What?\n");
	exit (1);
}
