/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-authenticate-fork.c - Fork a process and exec the given 
 * command.   Return the process id in *pid_out.
 */

#include <config.h>
#include "nautilus-authenticate.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static const int FORK_FAILED = -1;
static const int FORK_CHILD = 0;

gboolean
nautilus_authenticate_fork (const char	*command,
			    int	  	*pid_out)
{
	int pid;

	g_assert (pid_out);
	
	if (!pid_out)
		return FALSE;
	
	*pid_out = 0;
	
	/* Fork */
	pid = fork ();
	
	/* Failed */
	if (pid == FORK_FAILED)
		return FALSE;
	
	/* Child */
	if (pid == FORK_CHILD) {
		system (command);
		
		fprintf (stderr,"\n");
		fprintf (stdout,"\n");

		fflush (stderr);
		fflush (stdout);
		
		/* Exit child */
		_exit (0);
		
		/* Not reached */
		g_assert_not_reached ();
	}
	
	/* Parent */
	*pid_out = (int) pid;
	
	return TRUE;
}
