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
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include <config.h>
#include "eazel-install-tests.h"

InstallOptions*
init_default_install_configuration_test () {

	InstallOptions* rv;

	/* A temporary hack for now.  Will eventually read information from a
	 * configuration file
	 */
	 
	rv = g_new0 (InstallOptions, 1);
	rv->protocol = PROTOCOL_LOCAL;
	rv->mode_debug = TRUE;
	rv->mode_test = FALSE;
	rv->mode_verbose = FALSE;
	rv->mode_silent = FALSE;
	rv->mode_depend = FALSE;
	rv->mode_uninstall = FALSE;
	rv->mode_update = FALSE;
	rv->port_number = 0;
	rv->rpmrc_file = g_strdup ("/usr/lib/rpm/rpmrc");
	rv->pkg_list_file = g_strdup ("/home/pepper/tmp/packages.xml");
	rv->rpm_storage_dir = g_strdup ("/home/pepper/tmp");
	rv->install_tmpdir = g_strdup ("/tmp/eazel_install");

	g_assert (rv != NULL);
	return rv;

} /* end init_default_install_configuration */

void
dump_install_options (InstallOptions* iopts) {
	g_print ("***Begin iopts dump***\n");
	g_print ("protocol = %d\n", iopts->protocol);
	g_print ("debug = %d\n", iopts->mode_debug);
	g_print ("test = %d\n", iopts->mode_test);
	g_print ("verbose = %d\n", iopts->mode_verbose);
	g_print ("silent = %d\n", iopts->mode_silent);
	g_print ("depend = %d\n", iopts->mode_depend);
	g_print ("uninstall = %d\n", iopts->mode_uninstall);
	g_print ("update = %d\n", iopts->mode_update);
	g_print ("port_number = %d\n", iopts->port_number);
	g_print ("hostname = %s\n", iopts->hostname);
	g_print ("rpmrc_file = %s\n", iopts->rpmrc_file);
	g_print ("pkg_list = %s\n", iopts->pkg_list_file);
	g_print ("rpm_storage_dir = %s\n", iopts->rpm_storage_dir);
	g_print ("tmpdir = %s\n", iopts->install_tmpdir);
	g_print ("***End iopts dump***\n");
} /* end dump_install_options */

void 
dump_package_list (PackageData* pkg) {
	g_print ("***Begin pkg dump***\n");
	g_print ("name = %s\n", pkg->name);
	g_print ("version = %s\n", pkg->version);
	g_print ("minor = %s\n", pkg->minor);
	g_print ("archtype = %s\n", pkg->archtype);
	g_print ("bytesize = %d\n", pkg->bytesize);
	g_print ("summary = %s\n", pkg->summary);
	g_print ("***End pkg dump***\n");
} /* end dump_package_list */
