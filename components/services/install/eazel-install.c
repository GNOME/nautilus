/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 2000 Helix Code, Inc
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
 * 			Joe Shaw <joe@helixcode.com>
 * 			
 * Portions of the initial installer prototype we inspired and shamelessly ripped
 * from Joe Shaw's Helix Code install / updater, esp. the xml packaging / parsing 
 * format and the use of gnet. Without his work, this prototype would not be 
 * possible.
 *
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include <popt-gnome.h>
#include "eazel-install-lib.h"
#include "eazel-install-lib-rpm.h"
#include "eazel-install-lib-util.h"
#include "eazel-install-lib-xml.h"

static void show_usage (int exitcode, char* error);
static void show_license (int exitcode, char* error);

static void
show_usage (int exitcode, char* error) {
	fprintf (stderr, "Usage: eazel-install [options]\n"
			"Valid options are:\n"
			"	--help            : show help\n"
			"	--License         : show license\n"
			"	--local           : use local files\n"
			"	--http            : use http\n"
			"	--ftp             : use ftp\n"
			"	--test            : dry run - don't actually install\n"
			"	--uninstall       : uninstall the package list\n"
			"	--tmpdir <dir>    : temporary directory to store rpms\n"
			"	--server <url>)   : url of remote server\n\n"
			"Example: eazel-install --http --server www.eazel.com --tmpdir /tmp\n");
			
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
	char* popt_tmpdir;
	char* popt_server;
	char* remote_path;
		
	struct poptOption optionsTable[] = {
		{ "help", 'h', 0, NULL, 'h' }, 
		{ "License", 'L', 0, NULL, 'L' },
		{ "local", 'l', 0, NULL, 'l'},
		{ "http", 'w', 0, NULL, 'w' },
		{ "ftp", 'f', 0, NULL, 'f' },
		{ "test", 't', 0, NULL, 't' },
		{ "uninstall", 'u', 0, NULL, 'u' },
		{ "tmpdir", 'T', POPT_ARG_STRING, &popt_tmpdir, 'T' },
		{ "server", 's', POPT_ARG_STRING, &popt_server, 's' },
		{ NULL, '\0', 0, NULL, 0 }
	};

	retval = FALSE;
	USE_LOCAL = FALSE;
	USE_HTTP = FALSE;
	USE_FTP = FALSE;
	TEST_MODE = FALSE;
	UNINSTALL_MODE = FALSE;
	popt_server = NULL;
	popt_tmpdir = NULL;

	config_file = g_strdup ("/etc/eazel/services/eazel-services-config.xml");
	remote_path = g_strdup ("/package-list.xml");
	
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
			case 'T':
				break;
			case 's':
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


	if ( USE_FTP == TRUE ) {
		fprintf (stderr, "***FTP installs are not currently supported !***\n");
		exit (1);
	}
	else if (USE_HTTP == TRUE) {
		iopts->protocol = PROTOCOL_HTTP;
	}
	else {
		iopts->protocol = PROTOCOL_LOCAL;
	}

	if (iopts->mode_silent && iopts->mode_verbose == TRUE) {
		fprintf (stderr, "***You cannot specify verbose and silent modes\n"
						 "   at the same time !***\n");
		exit (1);
	}

	if (TEST_MODE == TRUE) {
		iopts->mode_test = TRUE;
	}

	if (popt_server) {
		iopts->hostname = g_strdup_printf ("%s", popt_server);
	}

	if (popt_tmpdir) {
		iopts->install_tmpdir = g_strdup_printf ("%s", popt_tmpdir);
	}

	if (!g_file_exists (iopts->install_tmpdir)) {
		int retval;

		g_print("Creating temporary download directory ...\n");

		retval = mkdir (iopts->install_tmpdir, 0755);
		if (retval < 0) {
			if (errno != EEXIST) {
				fprintf (stderr, "***Could not create temporary directory !***\n");
				exit (1);
			}
		}
	}
	
	if (iopts->protocol == PROTOCOL_HTTP) {

		g_print ("Getting package-list.xml from remote server ...\n");
		
		retval = http_fetch_xml_package_list (iopts->hostname,
											  iopts->port_number,
											  remote_path,
											  iopts->pkg_list_file);
		if (retval == FALSE) {
			fprintf (stderr, "***Unable to retrieve package-list.xml !***\n");
			exit (1);
		}
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
			fprintf (stderr, "***The install failed !***\n");
			exit (1);
		}
	}

	g_print ("Install completed normally...\n");
	g_free (config_file);
	g_free (iopts);
	exit (0);
}