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

gboolean
install_new_packages (InstallOptions* iopts) {

	GList *categories;
	gboolean rv;

	rpmSetVerbosity(RPMMESS_FATALERROR);
	rpmReadConfigFiles (iopts->rpmrc_file, NULL);

	categories = NULL;

	categories = fetch_xml_package_list_local (iopts->pkg_list_file);

	while (categories) {
		CategoryData* c = categories->data;
		GList* t = c->Packages;

		g_print ("Install Category - %s\n", c->name);
		while (t) {
			PackageData* pack = t->data;
			const char* pkg[2]; 
			int retval;

			retval = 0;
			
			pkg[0] = g_strdup_printf ("%s/%s", iopts->rpm_storage_dir, pack->rpm_name);
			pkg[1] = NULL;
			g_print ("Installing Package = %s\n", pkg[0]);
			retval = rpmInstall ("/", pkg, 0, INSTALL_UPGRADE | INSTALL_HASH,
								RPMPROB_FILTER_REPLACEPKG | RPMPROB_FILTER_OLDPACKAGE |
								RPMPROB_FILTER_REPLACEOLDFILES, NULL);
			if (retval == 0) {
				g_print ("Package install successful!\n");
				rv = TRUE;
			}
			else {
				g_print ("Package install failed !\n");
				rv = FALSE;
			}

			t = t->next;
		}
		categories = categories->next;
	}

	free_categories (categories);
	
	return rv;
} /* end install_new_packages */

gboolean 
install_update_packages (InstallOptions* iopts) {
	fprintf (stderr, "***Sorry update not supported yet!***\n");
	return TRUE;
} /* end install_update_packages */

gboolean 
uninstall_packages (InstallOptions* iopts) {
	fprintf (stderr, "***Sorry uninstall not supported yet!***\n");
	return TRUE;
} /* end install_new_packages */
