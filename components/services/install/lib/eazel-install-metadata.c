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
#include "eazel-install-metadata.h"

static gboolean create_default_configuration_metafile (const char* target_file);

InstallOptions*
init_default_install_configuration (const char* config_file) {

	InstallOptions* rv;
	xmlDocPtr doc;
	xmlNodePtr base;
	char* tmpbuf;

	if (!g_file_exists (config_file)) {
		gboolean rv;
		int retval;

		g_print("Creating default configuration file ...\n");
		
		retval = mkdir ("/etc/eazel/services", 0755);
		if (retval < 0) {
			if (errno != EEXIST) {
				fprintf (stderr, "***Could not create services directory !***\n");
				exit (1);
			}
		}

		rv = create_default_configuration_metafile(config_file);
		if (rv == FALSE) {
			fprintf(stderr, "***Could not create the default configuration file !***\n");
			exit (1);
		}
	}

	doc = xmlParseFile (config_file);
	
	if (doc == NULL) {
		fprintf (stderr, "***Unable to open config file!***\n");
		xmlFreeDoc (doc);
		g_assert (doc != NULL);
	}

	base = doc->root;
	if (base == NULL) {
		fprintf (stderr, "***The config file contains no data!***\n");
		xmlFreeDoc (doc);
		g_assert (base != NULL);
	}
	
	if (g_strcasecmp (base->name, "EAZEL_INSTALLER")) {
		fprintf (stderr, "***Cannot find the EAZEL_INSTALLER xmlnode!***\n");
		xmlFreeDoc (doc);
		g_error ("***Bailing from xmlparse!***\n");
	}

	rv = g_new0 (InstallOptions, 1);
	tmpbuf = xml_get_value (base, "PROTOCOL");
	if (tmpbuf[0] == 'l' || tmpbuf[0] == 'L') {
		rv->protocol = PROTOCOL_LOCAL;
	}
	else if (tmpbuf[0] == 'h' || tmpbuf[0] == 'H') {
		rv->protocol = PROTOCOL_HTTP;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'F') {
		rv->protocol = PROTOCOL_FTP;
	}
	tmpbuf = xml_get_value (base, "DEBUG");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_debug = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_debug = FALSE;
	}
	tmpbuf = xml_get_value (base, "DRY_RUN");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_test = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_test = FALSE;
	}
	tmpbuf = xml_get_value (base, "VERBOSE");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_verbose = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_verbose = FALSE;
	}
	tmpbuf = xml_get_value (base, "SILENT");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_silent = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_silent = FALSE;
	}
	tmpbuf = xml_get_value (base, "DEPEND");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_depend = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_depend = FALSE;
	}
	tmpbuf = xml_get_value (base, "UNINSTALL");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_uninstall = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_uninstall = FALSE;
	}
	tmpbuf = xml_get_value (base, "UPDATE");
	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv->mode_update = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv->mode_update = FALSE;
	}
	rv->port_number = atoi (xml_get_value (base, "PORT"));
	rv->hostname = g_strdup (xml_get_value (base, "HOSTNAME"));
	rv->rpmrc_file = g_strdup (xml_get_value (base, "RPMRC_FILE"));
	rv->pkg_list_file = g_strdup (xml_get_value (base, "PKG_LIST_FILE"));
	rv->rpm_storage_dir = g_strdup (xml_get_value (base, "RPM_STORAGE_DIR"));
	rv->install_tmpdir = g_strdup (xml_get_value (base, "TMPDIR"));	

	g_free (tmpbuf);
	xmlFreeDoc (doc);
	
	return rv;
} /* end init_default_install_configuration */

static gboolean
create_default_configuration_metafile (const char* target_file) {

	xmlDocPtr doc;
	xmlNodePtr tree;

	doc = xmlNewDoc ("1.0");
	doc->root = xmlNewDocNode (doc, NULL, "EAZEL_INSTALLER", NULL);
	tree = xmlNewChild (doc->root, NULL, "PROTOCOL", "LOCAL");
	tree = xmlNewChild (doc->root, NULL, "DEBUG", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "DRY_RUN", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "VERBOSE", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "SILENT", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "DEPEND", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "UNINSTALL", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "UPDATE", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "PORT", "80");
	tree = xmlNewChild (doc->root, NULL, "HOSTNAME", "10.1.1.5");
	tree = xmlNewChild (doc->root, NULL, "RPMRC_FILE", "/usr/lib/rpm/rpmrc");
	tree = xmlNewChild (doc->root, NULL, "PKG_LIST_FILE", "/etc/eazel/services/package-list.xml");
	tree = xmlNewChild (doc->root, NULL, "RPM_STORAGE_DIR", "/RPMS");
	tree = xmlNewChild (doc->root, NULL, "TMPDIR", "/tmp/eazel-install");

	if (doc == NULL) {
		fprintf (stderr, "***Error generating default configuration file !***\n");
		xmlFreeDoc (doc);
		exit (1);
	}

	xmlSaveFile (target_file, doc);
	xmlFreeDoc (doc);

	return TRUE;

} /* end create_default_configuration_metafile */

