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

#include "eazel-install-lib-rpm.h"

gboolean
install_new_packages (InstallOptions* iopts) {

	GList *categories;
	gboolean rv;
	int installFlags, interfaceFlags, probFilter;
	
	categories = NULL;
	installFlags = 0;
	interfaceFlags = 0;
	probFilter = 0;
	
	if (iopts->mode_test == TRUE) {
		installFlags |= RPMTRANS_FLAG_TEST;
	}

	if (iopts->mode_update == TRUE) {
		interfaceFlags |= INSTALL_UPGRADE;
	}

	if (iopts->mode_verbose == TRUE) {
		interfaceFlags |= INSTALL_HASH;
		rpmSetVerbosity (RPMMESS_VERBOSE);
	}
	else {
		rpmSetVerbosity (RPMMESS_NORMAL);
	}

	/* FIXME This needs to be setup as an option.  Forcing everything right now. */
	probFilter |= RPMPROB_FILTER_REPLACEPKG |
				  RPMPROB_FILTER_REPLACEOLDFILES |
				  RPMPROB_FILTER_REPLACENEWFILES |
				  RPMPROB_FILTER_OLDPACKAGE;
	
	rpmReadConfigFiles (iopts->rpmrc_file, NULL);

	g_print ("Reading the install package list ...\n");
	categories = parse_local_xml_package_list (iopts->pkg_list_file);

	while (categories) {
		CategoryData* c = categories->data;
		GList* t = c->Packages;

		g_print ("Install Category - %s\n", c->name);
		while (t) {
			PackageData* pack = t->data;
			const char* pkg[2]; 
			char *tmpbuf;
			int retval;

			retval = 0;
			
			tmpbuf = g_strdup_printf ("%s/%s-%s-%s.%s.rpm", iopts->install_tmpdir,
															pack->name,
															pack->version,
															pack->minor,
															pack->archtype);

			if (iopts->protocol == PROTOCOL_HTTP) {
				int rv;
				char* hostname;
				char* rpmname;
				char* targetname;
				char* url;

				hostname = g_strdup ("10.1.1.5");
				rpmname = g_strdup_printf ("%s-%s-%s.%s.rpm", pack->name,
															 pack->version,
															 pack->minor,
															 pack->archtype);
	
				targetname = g_strdup_printf ("%s/%s", iopts->install_tmpdir, rpmname);
				url = g_strdup_printf ("http://%s%s/%s", hostname,
															 iopts->rpm_storage_dir,
															 rpmname);
				
				g_print ("Downloading %s...\n", rpmname);
				rv = urlGetFile (url, targetname);
				if (rv != 0) {
					fprintf (stderr, "***Failed to retreive %s !***\n", url);
					exit (1);
				}

				g_free (hostname);
				g_free (rpmname);
				g_free (targetname);
				g_free (url);

			}
			
            pkg[0] = tmpbuf;
			pkg[1] = NULL;
			g_print ("Installing %s\n", pack->summary);
			retval = rpmInstall ("/", pkg, installFlags, interfaceFlags,
								probFilter, NULL);
			if (retval == 0) {
				g_print ("Package install successful !\n");
				rv = TRUE;
			}
			else {
				g_print ("Package install failed !\n");
				rv = FALSE;
			}
			g_free(tmpbuf);
			t = t->next;
		}
		categories = categories->next;
	}

	free_categories (categories);
	
	return rv;
} /* end install_new_packages */

gboolean 
uninstall_packages (InstallOptions* iopts) {
	GList *categories;
	gboolean rv;
	int uninstallFlags, interfaceFlags;
	
	categories = NULL;
	uninstallFlags = 0;
	interfaceFlags = 0;
	
	if (iopts->mode_test == TRUE) {
		uninstallFlags |= RPMTRANS_FLAG_TEST;
	}

	if (iopts->mode_verbose == TRUE) {
		interfaceFlags |= INSTALL_HASH;
		rpmSetVerbosity (RPMMESS_VERBOSE);
	}
	else {
		rpmSetVerbosity (RPMMESS_NORMAL);
	}

	rpmReadConfigFiles (iopts->rpmrc_file, NULL);

	g_print ("Reading the uninstall package list ...\n");
	categories = parse_local_xml_package_list (iopts->pkg_list_file);

	while (categories) {
		CategoryData* c = categories->data;
		GList* t = c->Packages;

		g_print ("Uninstall Category - %s\n", c->name);
		while (t) {
			PackageData* pack = t->data;
			const char* pkg[2]; 
			char *tmpbuf;
			int retval;

			retval = 0;
			if (g_strcasecmp (pack->archtype, "src") != 0) {

				tmpbuf = g_strdup_printf ("%s-%s-%s", pack->name,
													  pack->version,
													  pack->minor);
            	pkg[0] = tmpbuf;
				pkg[1] = NULL;
				g_print ("Uninstalling %s\n", pack->summary);
				retval = rpmErase ("/", pkg, uninstallFlags, interfaceFlags);

				if (retval == 0) {
					g_print ("Package uninstall successful !\n");
					rv = TRUE;
				}
				else {
					g_print ("Package uninstall failed !\n");
					rv = FALSE;
				}
				g_free(tmpbuf);
			}
			else {
				tmpbuf = g_strdup_printf ("%s-%s-%s", pack->name,
													  pack->version,
													  pack->minor);
  				g_print ("%s seems to be a source package.  Skipping ...\n", tmpbuf);
  				g_free(tmpbuf);
  			}
			t = t->next;
		}
		categories = categories->next;
	}

	free_categories (categories);
	
	return rv;

} /* end install_new_packages */
