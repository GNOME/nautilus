/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2001 Eazel, Inc
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <libtrilobite/libtrilobite.h>
#include <unistd.h>

CORBA_ORB orb;
CORBA_Environment ev;
int cli_result = 0;

/* Popt stuff */
int     arg_command = 0;

static const struct poptOption options[] = {
	{"command", '\0', POPT_ARG_INT, &arg_command, 0, N_("Command to run"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static char *
get_password_dude (TrilobiteRootHelper *helper, const char *prompt, void *user_data)
{
	char * passwd;

	passwd = getpass ("root password: ");
	return g_strdup (passwd);
}

int main(int argc, char *argv[]) {
	poptContext ctxt;
	TrilobiteRootHelper *helper;
	int fd;
	GList *args = NULL;
	char *str;

	CORBA_exception_init (&ev);

	/* Seems that bonobo_main doens't like
	   not having gnome_init called, dies in a
	   X call, yech */

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	trilobite_init ("Eazel Root Helper Test", "1.0", NULL, options, argc, argv);
	ctxt = trilobite_get_popt_context ();

	helper = trilobite_root_helper_new ();
 
	while ((str = poptGetArg (ctxt)) != NULL) {
		args = g_list_prepend (args, str);
	}

	bonobo_activate ();

	gtk_signal_connect (GTK_OBJECT (helper), 
			    "need_password", 
			    GTK_SIGNAL_FUNC (get_password_dude),
			    NULL);
	
	g_message ("Calling start");
	if (trilobite_root_helper_start (helper) == 0) {
		TrilobiteRootHelperStatus res;
		
		g_message ("Calling run");
		res = trilobite_root_helper_run (helper, arg_command, args, &fd);	

		switch (res) {
		case TRILOBITE_ROOT_HELPER_SUCCESS:
			if (fd>0) {
				char b;
				while (read (fd, &b, 1)) {
					putchar (b);
				}
			} else {
				g_warning ("success, but no fd");
			}
			break;
		case TRILOBITE_ROOT_HELPER_NO_USERHELPER:
			g_error ("No userhelper");
			break;
		case TRILOBITE_ROOT_HELPER_NEED_PASSWORD:
			g_error ("Need password");
			break;
		case TRILOBITE_ROOT_HELPER_BAD_PASSWORD:
			g_error ("Bad password, try again...");
			break;
		case TRILOBITE_ROOT_HELPER_LOST_PIPE:
			g_error ("Lost pipe");
			break;
		case TRILOBITE_ROOT_HELPER_BAD_ARGS:
			g_error ("Bad args");
			break;
		case TRILOBITE_ROOT_HELPER_BAD_COMMAND:
			g_error ("Bad command");
			break;
		case TRILOBITE_ROOT_HELPER_INTERNAL_ERROR:
			g_error ("Internal error");
			break;
		}
	} else {
		g_error ("Cannot start root helper");
	}


	/* Corba cleanup */
	CORBA_exception_free (&ev);

	return cli_result;
};
