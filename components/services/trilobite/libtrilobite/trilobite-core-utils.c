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
		/* make stdin/stdout/stderr use the pipes */
		dup2 (pipe_in[0], 0);
		dup2 (pipe_out[1], 1);
		dup2 (pipe_err[1], 2);
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
	*stdin_fd = pipe_in[1];
	*stdout_fd = pipe_out[0];
	*stderr_fd = pipe_err[0];

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
