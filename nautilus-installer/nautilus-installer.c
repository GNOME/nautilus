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
 *
 */

/* nautilus-installer - A very simple bootstrap installer for nautilus and
 * its dependancies.
 */

#include "eazel-install-types.h"
#include "eazel-install-protocols.h"
#include "eazel-install-rpm-glue.h"
#include "eazel-install-xml-package-list.h"
#include "eazel-install-metadata.h"
#include "helixcode-utils.h"
#include <config.h>
#include <popt-gnome.h>

/* Here are the values to change if the internal server changes location or 
 * storage locations. You can also change these if you want to play with the
 * installer locally.
 */

#define REMOTE_SERVER_NAME "10.1.1.5"
#define REMOTE_PKG_LIST_PATH "/package-list.xml"
#define REMOTE_RPM_DIR "/RPMS"
#define PATH_TO_RPMRC "/usr/lib/rpm/rpmrc"
#define LOCAL_PKG_LIST_PATH "/tmp/package-list.xml"
#define DEFAULT_TMP_DIR "/tmp/eazel-install"
	
/* Forward Function Declarations */
static void show_usage (int exitcode, char* error);
static void show_license (int exitcode, char* error);
static TransferOptions* init_default_topts (void);
static InstallOptions* init_default_iopts (void);
static void create_temporary_directory (const char* tmpdir);
static void fetch_remote_package_list (const char* pkg_list,
                                       TransferOptions* topts);

/* Show options available for the bootstrap installer */
static void
show_usage (int exitcode, char* error) {
	fprintf (stderr, "Usage: nautilus-installer [options]\n"
			"Valid options are:\n"
			"	--help                   : show help\n"
			"	--License                : show license\n"
			"	--test                   : dry run - don't actually install\n"
			"	--force                  : force package to install/uninstall\n"
			"	--tmpdir <dir>           : temporary directory to store rpms\n"
			"\nExample: nautilus-installer --force --tmpdir /tmp\n\n");
			
	if (error) {
		fprintf (stderr, "%s\n", error);
	}

	exit (exitcode);

} /* end usage */

/* Print the GPL license if --license option is given */
static void
show_license (int exitcode, char* error) {
	fprintf (stderr, "nautilus-installer: a bootstrap installer for nautilus and its dependancies\n\n"
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

/* Initialize the InstallOptions struct with default values */
static InstallOptions*
init_default_iopts () {

	InstallOptions* rv;

	rv = g_new0 (InstallOptions, 1);
	rv->protocol = PROTOCOL_HTTP;
	rv->pkg_list = g_strdup (LOCAL_PKG_LIST_PATH);
	rv->mode_verbose = TRUE;
	rv->mode_silent = FALSE;
	rv->mode_debug = FALSE;
	rv->mode_test = FALSE;
	rv->mode_force = FALSE;
	rv->mode_depend = FALSE;
	rv->mode_update = TRUE;
	rv->mode_uninstall = FALSE;
	rv->mode_downgrade = FALSE;

	g_assert (rv != NULL);

	return rv;

} /* end init_default_iopts */

/* Initialize the TransferOptions struct with default values */
static TransferOptions*
init_default_topts () {

	TransferOptions* rv;

	rv = g_new0 (TransferOptions, 1);
	rv->hostname = g_strdup (REMOTE_SERVER_NAME);
	rv->port_number = 80;
	rv->pkg_list_storage_path = g_strdup (REMOTE_PKG_LIST_PATH);
	rv->rpm_storage_path = g_strdup (REMOTE_RPM_DIR);
	rv->tmp_dir = g_strdup (DEFAULT_TMP_DIR);
	rv->rpmrc_file = g_strdup (PATH_TO_RPMRC);

	g_assert (rv != NULL);

	return rv;

} /* end init_default_topts */

/* Create the local storage location if it does not currently exist */
static void
create_temporary_directory (const char* tmpdir) {

	int retval;

	g_print("Creating temporary download directory ...\n");

	retval = mkdir (tmpdir, 0755);
	if (retval < 0) {
		if (errno != EEXIST) {
			g_error ("*** Could not create temporary directory ! ***\n");
		}
	}
} /* end create_temporary_directory */

/* Fetch the package-list.xml file from the remote server to know what needs
 * to be installed.
 */
static void
fetch_remote_package_list (const char* pkg_list, TransferOptions* topts) {

	gboolean retval;

	g_print ("Getting package-list.xml from remote server ...\n");

	retval = http_fetch_xml_package_list (topts->hostname,
                                              topts->port_number,
                                              topts->pkg_list_storage_path,
                                              pkg_list);
	if (retval == FALSE) {
		g_error ("*** Unable to retrieve package-list.xml! ***\n");
	}
} /* end fetch_remote_package_list */

int
main (int argc, char* argv[]) {
	char opt;
	gboolean retval;
	gboolean TEST_MODE, FORCE_MODE;
	InstallOptions* iopts;
	TransferOptions* topts;
	poptContext pctx;
	char* config_file;
	char* popt_tmpdir;
		
	struct poptOption optionsTable[] = {
		{ "help", 'h', 0, NULL, 'h' }, 
		{ "License", 'L', 0, NULL, 'L' },
		{ "force", 'F', 0, NULL, 'F' },
		{ "test", 't', 0, NULL, 't' },
		{ "tmpdir", 'T', POPT_ARG_STRING, &popt_tmpdir, 'T' },
		{ NULL, '\0', 0, NULL, 0 }
	};

	retval = FALSE;
	FORCE_MODE = FALSE;
	TEST_MODE = FALSE;
	popt_tmpdir = NULL;

	pctx = poptGetContext ("nautilus-installer", argc, argv, optionsTable, 0);

	while ( (opt = poptGetNextOpt (pctx)) >= 0) {
		switch (opt) {
			case 'h':
				show_usage (0, NULL);
				break;
			case 'L':
				show_license (0, NULL);
				break;
			case 'F':
				FORCE_MODE = TRUE;
				break;
			case 't':
				TEST_MODE = TRUE;
				break;
			case 'T':
				break;
		}
	}

	if (opt < -1) {

		/* Error generated during option processing */

		fprintf (stderr, "*** Option = %s: %s ***\n",
				poptBadOption (pctx, POPT_BADOPTION_NOALIAS),
				poptStrerror (opt));
		exit (1);
	}

	poptFreeContext (pctx);

	/* This is the for internal usage only warning.  It will be removed once
         * we are ready for primetime.
         */
	g_print ("WARNING: The nautilus bootstrap installer is for internal\n"
                 "use at Eazel, Inc. only.  It should not be redistributed\n"
                 "until we are ready to release Nautilus 1.0.  This installer\n"
                 "is by no means complete in it's current form so be prepared\n"                 "for unpredictable behavior.\n");

	retval = check_for_root_user ();
	if (retval == FALSE) {
		g_error ("*** You must run nautilus-installer as root! ***\n");
	}

	retval = check_for_redhat ();
	if (retval == FALSE) {
		g_error ("*** nautilus-installer can only be used on RedHat! ***\n");
	}

	/* Populate iopts and topts with eazel defaults */
	iopts = init_default_iopts ();
	topts = init_default_topts ();

	if (TEST_MODE == TRUE) {
		iopts->mode_test = TRUE;
	}
	
	if (FORCE_MODE == TRUE) {
		iopts->mode_force = TRUE;
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

	retval = install_new_packages (iopts, topts);
	if (retval == FALSE) {
		g_error ("*** The install failed! ***\n");
	}

	g_print ("Install completed normally...\n");
	g_free (iopts);
	g_free (topts);
	exit (0);
}

