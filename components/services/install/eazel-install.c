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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-configuration.xml
 * file and install a services generated packages.xml.
 */

#include "eazel-install-lib.h"

static void show_usage (int exitcode, char* error);
static void show_license (int exitcode, char* error);

static void
show_usage (int exitcode, char* error) {
	fprintf (stderr, "Usage: eazel-install [options]\n"
			"Valid options are:\n"
			"	-h --help       : show help\n"
			"	-L --License    : show license\n"
			"	-l --local      : use local file\n"
			"	-w --http       : use http\n"
			"	-f --ftp        : use ftp\n"
			"	-t --test       : dry run - don't actually install\n"
			"	-u --uninstall	: uninstall the package list\n");
			
	if (error) {
		fprintf (stderr, "%s\n", error);
	}

	exit (exitcode);

} /* end usage */

static void
show_license (int exitcode, char* error) {
	fprintf (stderr, "eazel-install: a quick and powerful remote installation utility.\n\n"
			"Copyright (C) 2000 Eazel, Inc.\n\n"
			"This program is free software; you can redistribute it and/or\n"
			"modify it under the terms of the GNU General Public License as\n"
			"published by the Free Software Foundation; either version 2 of the\n"
			"License, or (at your option) any later version.\n\n"
			"This program is distributed in the hope that it will be useful,\n"
			"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
			"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
			"General Public License for more details.\n\n"
			"You should have received a copy of the GNU General Public License\n"
			"along with this program; if not, write to the Free Software\n"
			"Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\n");

	if (error) {
		fprintf(stderr, "%s\n", error);
	}

	exit(exitcode);

} /* end show_license */

int
main (int argc, char* argv[]) {
	char opt;
	gboolean retval;
	gboolean USE_LOCAL, USE_HTTP, USE_FTP, UNINSTALL_MODE, TEST_MODE;
	InstallOptions *iopts;
	poptContext pctx;
	char* config_file;
		
	struct poptOption optionsTable[] = {
		{ "help", 'h', 0, NULL, 'h' }, 
		{ "License", 'L', 0, NULL, 'L' },
		{ "local", 'l', 0, NULL, 'l'},
		{ "http", 'w', 0, NULL, 'w' },
		{ "ftp", 'f', 0, NULL, 'f' },
		{ "test", 't', 0, NULL, 't' },
		{ "uninstall", 'u', 0, NULL, 'u' },
		{ NULL, '\0', 0, NULL, 0 }
	};

	retval = FALSE;
	USE_LOCAL = FALSE;
	USE_HTTP = FALSE;
	USE_FTP = FALSE;
	TEST_MODE = FALSE;
	UNINSTALL_MODE = FALSE;

	config_file = g_strdup ("/etc/eazel/services/eazel-services-config.xml");
/*
	if (argc < 2) {
		show_usage (1, "\n***You must specify the protocol!***\n");
	}
*/
	pctx = poptGetContext ("eazel-install", argc, argv, optionsTable, 0);

	while ( (opt = poptGetNextOpt (pctx)) >= 0) {
		switch (opt) {
			case 'h':
				show_usage (0, NULL);
				break;
			case 'L':
				show_license (0, NULL);
				break;
			case 'l':
				USE_LOCAL = TRUE;
				break;
			case 'w':
				USE_HTTP = TRUE;
				break;
			case 'f':
				USE_FTP = TRUE;
				break;
			case 't':
				TEST_MODE = TRUE;
				break;
			case 'u':
				UNINSTALL_MODE = TRUE;
				break;
		}
	}

	if (opt < -1) {

		/* Error generated during option processing */

		fprintf (stderr, "***Option = %s: %s***\n",
				poptBadOption (pctx, POPT_BADOPTION_NOALIAS),
				poptStrerror (opt));
		exit (1);
	}

	poptFreeContext (pctx);

	if ( USE_FTP || USE_HTTP == TRUE ) {
		fprintf (stderr, "***Only local installs are currently supported right now!***\n");
		exit (1);
	}

	retval = check_for_root_user ();
	if (retval == FALSE) {
		fprintf (stderr, "***You must run eazel-install as root!***\n");
		exit (1);
	}

	retval = check_for_redhat ();
	if (retval == FALSE) {
		fprintf (stderr, "***eazel-install can only be used on RedHat!***\n");
		exit (1);
	}

	/* Initialize iopts with defaults from the configuration file */
	g_print ("Reading the eazel services configuration ...\n");
	iopts = init_default_install_configuration (config_file);

/*
	if (iopts->mode_debug == TRUE) {
		dump_install_options (iopts);
	}
*/
	/* Do the install */

	if (iopts->mode_silent && iopts->mode_verbose == TRUE) {
		fprintf (stderr, "***You cannot specify verbose and silent modes\n"
						 "   at the same time !\n");
		exit (1);
	}

	if (TEST_MODE == TRUE) {
		iopts->mode_test = TRUE;
	}
	
	if (UNINSTALL_MODE == TRUE) {
		iopts->mode_uninstall = TRUE;

		retval = uninstall_packages (iopts);
		if (retval == FALSE) {
			fprintf (stderr, "***The uninstall failed !***\n");
			exit (1);
		}
	}
	else {
		retval = install_new_packages (iopts);
		if (retval == FALSE) {
			fprintf (stderr, "***The install failed!***\n");
			exit (1);
		}
	}

	g_print ("***Install completed normally.***\n");
	g_free (config_file);
	g_free (iopts);
	exit (0);
}