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
 *          Joe Shaw <joe@helixcode.com>
 *
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-types.h"
#include "eazel-install-protocols.h"
#include "eazel-install-rpm-glue.h"
#include "eazel-install-xml-package-list.h"
#include "eazel-install-metadata.h"
#include <libtrilobite/helixcode-utils.h>
#include <config.h>
#include <popt-gnome.h>

#define PACKAGE_FILE_NAME "package-list.xml"
#define DEFAULT_CONFIG_FILE "/var/eazel/services/eazel-services-config.xml"
	

static void show_usage (int exitcode, char* error);
static void show_license (int exitcode, char* error);
static void generate_new_package_list (const char* popt_genpkg_file,
                                       const char* config_file);
static void create_temporary_directory (const char* tmpdir);
static void fetch_remote_package_list (const char* pkg_list,
                                       TransferOptions* topts);

static void
show_usage (int exitcode, char* error) {
	fprintf (stderr, "Usage: eazel-install [options]\n"
			"Valid options are:\n"
			"	--help                   : show help\n"
			"	--License                : show license\n"
			"	--local                  : use local files\n"
			"	--http                   : use http\n"
			"	--ftp                    : use ftp\n"
			"	--verbose                : give more information during processing\n"
			"	--log <file>             : redirect all information to log file.\n"
			"	--test                   : dry run - don't actually install\n"
			"	--force                  : force package to install/uninstall\n"
			"	--uninstall              : uninstall the package list\n"
			"	--tmpdir <dir>           : temporary directory to store rpms\n"
			"	--server <url>)          : url of remote server\n"
			"        --new-pkg-list <file>    : generate xml package list from template\n"
			"\nExample: eazel-install --http --server www.eazel.com --tmpdir /tmp\n\n");
			
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

static void
generate_new_package_list (const char* popt_genpkg_file,
                           const char* config_file) {

	gboolean retval;

	retval = generate_xml_package_list (popt_genpkg_file, config_file);

	if (!retval) {
		g_error (_("*** Could not generate xml package list! ***\n"));
	}

	g_print (_("XML package list successfully generated ...\n"));
	exit (1);
} /* end generate_new_package_list */

static void
create_temporary_directory (const char* tmpdir) {

	int retval;

	g_print (_("Creating temporary download directory ...\n"));

	retval = mkdir (tmpdir, 0755);
	if (retval < 0) {
		if (errno != EEXIST) {
			g_error (_("*** Could not create temporary directory! ***\n"));
		}
	}
} /* end create_temporary_directory */

static void
fetch_remote_package_list (const char* pkg_list, TransferOptions* topts) {

	gboolean retval;
	char* url;

	g_print (_("Getting package-list.xml from remote server ...\n"));

	url = g_strdup_printf ("http://%s%s", topts->hostname,
                                topts->pkg_list_storage_path);

	retval = http_fetch_remote_file (url, pkg_list);

	if (!retval) {
		g_free (url);
		g_error ("*** Unable to retrieve package-list.xml! ***\n");
	}
	g_free (url);
} /* end fetch_remote_package_list */

int
main (int argc, char* argv[]) {
	char opt;
	gboolean retval;
	gboolean USE_LOCAL, USE_HTTP, USE_FTP, UNINSTALL_MODE, DOWNGRADE_MODE, TEST_MODE, FORCE_MODE, VERBOSE_MODE, LOGGING_MODE;
	InstallOptions* iopts;
	TransferOptions* topts;
	poptContext pctx;
	char* config_file;
	char* target_file;
	char* popt_log_file;
	char* popt_tmpdir;
	char* popt_server;
	char* popt_genpkg_file;
		
	struct poptOption optionsTable[] = {
		{ "help", 'h', 0, NULL, 'h' }, 
		{ "License", 'L', 0, NULL, 'L' },
		{ "local", 'l', 0, NULL, 'l'},
		{ "http", 'w', 0, NULL, 'w' },
		{ "ftp", 'f', 0, NULL, 'f' },
		{ "verbose", 'v', 0, NULL, 'v' },
		{ "log", 'S', POPT_ARG_STRING, &popt_log_file, 'S' },
		{ "force", 'F', 0, NULL, 'F' },
		{ "test", 't', 0, NULL, 't' },
		{ "uninstall", 'u', 0, NULL, 'u' },
		{ "downgrade", 'D', 0, NULL, 'D' },
		{ "tmpdir", 'T', POPT_ARG_STRING, &popt_tmpdir, 'T' },
		{ "server", 's', POPT_ARG_STRING, &popt_server, 's' },
		{ "genpkg_list", 'g', POPT_ARG_STRING, &popt_genpkg_file, 'g' },
		{ NULL, '\0', 0, NULL, 0 }
	};

	retval = FALSE;
	USE_LOCAL = FALSE;
	USE_HTTP = FALSE;
	USE_FTP = FALSE;
	VERBOSE_MODE = FALSE;
	LOGGING_MODE = FALSE;
	FORCE_MODE = FALSE;
	TEST_MODE = FALSE;
	UNINSTALL_MODE = FALSE;
	DOWNGRADE_MODE = FALSE;
	popt_log_file = NULL;
	popt_server = NULL;
	popt_tmpdir = NULL;
	popt_genpkg_file = NULL;

	config_file = g_strdup (DEFAULT_CONFIG_FILE);
	target_file = g_strdup (PACKAGE_FILE_NAME);
	
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
			case 'v':
				VERBOSE_MODE = TRUE;
				break;
			case 'S':
				LOGGING_MODE = TRUE;
				break;
			case 'F':
				FORCE_MODE = TRUE;
				break;
			case 't':
				TEST_MODE = TRUE;
				break;
			case 'u':
				UNINSTALL_MODE = TRUE;
				break;
			case 'D':
				DOWNGRADE_MODE = TRUE;
				break;
			case 'T':
				break;
			case 's':
				break;
			case 'g':
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
	if (!retval) {
		g_error (_("*** You must run eazel-install as root! ***\n"));
	}

	retval = check_for_redhat ();
	if (!retval) {
		g_error (_("*** eazel-install can only be used on RedHat! ***\n"));
	}

	if ( DOWNGRADE_MODE ) {
		g_error (_("*** Downgrade Mode not supported yet! ***\n"));
	}

	/* Initialize iopts and topts with defaults from the configuration file */
	g_print (_("Reading the eazel services configuration ...\n"));
	iopts = init_default_install_configuration (config_file);
	topts = init_default_transfer_configuration (config_file);

	if (popt_genpkg_file) {
		generate_new_package_list (popt_genpkg_file, target_file);
	}

	if ( LOGGING_MODE ) {
		g_error (_("*** Logging not currently supported! ***\n"));
		exit (1);
	}

	if ( USE_FTP ) {
		g_error (_("*** FTP installs are not currently supported! ***\n"));
		exit (1);
	}
	else if (USE_HTTP) {
		iopts->protocol = PROTOCOL_HTTP;
	}
	else {
		iopts->protocol = PROTOCOL_LOCAL;
	}

	if (VERBOSE_MODE) {
		iopts->mode_verbose = TRUE;
	}

	if (TEST_MODE) {
		iopts->mode_test = TRUE;
	}
	
	if (FORCE_MODE) {
		iopts->mode_force = TRUE;
	}

	if (popt_server) {
		topts->hostname = g_strdup_printf ("%s", popt_server);
	}

	if (popt_tmpdir) {
		topts->tmp_dir = g_strdup_printf ("%s", popt_tmpdir);
	}

	if (!g_file_exists (topts->tmp_dir)) {
		create_temporary_directory (topts->tmp_dir);
	}
	
	if (iopts->protocol == PROTOCOL_HTTP) {
		fetch_remote_package_list (iopts->pkg_list, topts);
	}

	if (UNINSTALL_MODE) {
		iopts->mode_uninstall = TRUE;

		retval = uninstall_packages (iopts, topts);
		if (!retval) {
			g_error (_("*** The uninstall failed! ***\n"));
		}
	}
	else {
		retval = install_new_packages (iopts, topts);
		if (!retval) {
			g_error (_("*** The install failed! ***\n"));
		}
	}

	g_print (_("Transaction completed normally...\n"));
	g_free (config_file);
	g_free (iopts);
	g_free (topts);
	exit (0);
}

