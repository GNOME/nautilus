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

/* nautilus-authenticate.c - Main for helper utility to authenticate a
 * user and execute a priviledge command on their behalf.
 */

#include <config.h>
#include "nautilus-authenticate.h"

#include <libnautilus-extensions/nautilus-password-dialog.h>

#include <libgnomeui/gnome-init.h>


#include <stdio.h>
#include <unistd.h>

extern char gnome_do_not_create_directories;

int main (int argc, char *argv[])
{
	GtkWidget * password_dialog = NULL;
	
	gchar* command = NULL;

	int rv = 1;

	g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);

	gnome_do_not_create_directories = 1;
	
	gnome_init ("PrivilegedAuthentication", "1.0", argc, argv);

	if (argc > 1)
	{
		GString *str = g_string_new ("");
		guint i;
		
		for(i = 1; i < argc; i++)
		{
			if (i > 1) 
				g_string_append(str, " ");
			
			g_string_append (str, argv[i]);
		}
		
		command = g_strndup (str->str, str->len);
		
		g_string_free (str, TRUE);
	}
	
	if (!command)
		command = g_strdup("");
	
	password_dialog = nautilus_password_dialog_new ("Privileged Command Execution",
							NULL,
							"root",
							"",
							TRUE);
	
	g_free (command);
	
	if (nautilus_password_dialog_run_and_block (NAUTILUS_PASSWORD_DIALOG (password_dialog))) {
		char *username;
		char *password;
		
		username = nautilus_password_dialog_get_username (NAUTILUS_PASSWORD_DIALOG (password_dialog));
		password = nautilus_password_dialog_get_password (NAUTILUS_PASSWORD_DIALOG (password_dialog));
		
		if (nautilus_authenticate_authenticate (username, password))
		{
			/* Free the password right away to blow it away from memory. */
			if (password) {
				g_free(password);
				
				password = NULL;
			}
			
			if (setuid (0) == 0) {
				gint pid = 0;
				
				if (!nautilus_authenticate_fork (command, &pid))
					perror("fork");			
			}
			else {
				perror ("setuid(0)");			
			}
		}
		else {
			fprintf (stderr, 
				 "Authentication for user '%s' failed.\n\n",
				 username);
		}

		if (username) {
			g_free(username);
			username = NULL;
		}
		
		if (password) {
			g_free(password);
			password = NULL;
		}
	}

	return rv;
}
