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

#define EAZEL_SERVICES_DIR_HOME "/var/eazel"
#define EAZEL_SERVICES_DIR EAZEL_SERVICES_DIR_HOME "/services"

static void create_default_metadata (const char* config_file);
static gboolean create_default_configuration_metafile (const char* target_file);
static gboolean xml_doc_sanity_checks (xmlDocPtr doc);
static URLType get_urltype_from_string (char* tmpbuf);
static gboolean get_boolean_value_from_string (char* tmpbuf);

static void
create_default_metadata (const char* config_file) {

	gboolean rv;
	int retval;

	g_print (_("Creating default configuration file ...\n"));

	/* Ensure our services dir exists */
	if (! g_file_test (EAZEL_SERVICES_DIR, G_FILE_TEST_ISDIR)) {
		if (! g_file_test (EAZEL_SERVICES_DIR_HOME, G_FILE_TEST_ISDIR)) {
			retval = mkdir (EAZEL_SERVICES_DIR_HOME, 0755);		       
			if (retval < 0) {
				if (errno != EEXIST) {
					g_error (_("*** Could not create services directory (%s)! ***\n"), EAZEL_SERVICES_DIR_HOME);
				}
			}
		}

		retval = mkdir (EAZEL_SERVICES_DIR, 0755);
		if (retval < 0) {
			if (errno != EEXIST) {
				g_error (_("*** Could not create services directory (%s)! ***\n"), EAZEL_SERVICES_DIR);
			}
		}
	}

	rv = create_default_configuration_metafile (config_file);
	if (rv == FALSE) {
		g_error (_("*** Could not create the default configuration file! ***\n"));
	}
} /* end create_default_metadata */

static gboolean
create_default_configuration_metafile (const char* target_file) {

	xmlDocPtr doc;
	xmlNodePtr tree;

	doc = xmlNewDoc ("1.0");
	doc->root = xmlNewDocNode (doc, NULL, "EAZEL_INSTALLER", NULL);
	tree = xmlNewChild (doc->root, NULL, "PROTOCOL", "LOCAL");
	tree = xmlNewChild (doc->root, NULL, "PKG_LIST", "/var/eazel/services/package-list.xml");
	tree = xmlNewChild (doc->root, NULL, "VERBOSE", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "SILENT", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "DEBUG", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "DRY_RUN", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "FORCE", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "DEPEND", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "UPDATE", "TRUE");
	tree = xmlNewChild (doc->root, NULL, "UNINSTALL", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "DOWNGRADE", "FALSE");
	tree = xmlNewChild (doc->root, NULL, "PORT", "80");
	tree = xmlNewChild (doc->root, NULL, "HOSTNAME", "10.1.1.5");
	tree = xmlNewChild (doc->root, NULL, "PKG_LIST_STORAGE_PATH", "/package-list.xml");
	tree = xmlNewChild (doc->root, NULL, "RPM_STORAGE_PATH", "/RPMS");
	tree = xmlNewChild (doc->root, NULL, "TMP_DIR", "/tmp/eazel-install");
	tree = xmlNewChild (doc->root, NULL, "TRANSACTION_DIR", "/var/eazel/services/transaction");
	tree = xmlNewChild (doc->root, NULL, "RPMRC_FILE", "/usr/lib/rpm/rpmrc");

	if (doc == NULL) {
		xmlFreeDoc (doc);
		g_error (_("*** Error generating default configuration file! ***\n"));
	}

	xmlSaveFile (target_file, doc);
	xmlFreeDoc (doc);

	return TRUE;

} /* end create_default_configuration_metafile */

static gboolean
xml_doc_sanity_checks (xmlDocPtr doc) {

	xmlNodePtr base;

	if (doc == NULL) {
		xmlFreeDoc (doc);
		g_warning (_("*** Unable to open config file! ***\n"));
		return FALSE;
	}

	base = doc->root;
	if (base == NULL) {
		xmlFreeDoc (doc);
		g_warning (_("*** The config file contains no data! ***\n"));
		return FALSE;
	}
	
	if (g_strcasecmp (base->name, "EAZEL_INSTALLER")) {
		g_print (_("*** Cannot find the EAZEL_INSTALLER xmlnode! ***\n"));
		xmlFreeDoc (doc);
		g_warning (_("*** Bailing from xmlparse! ***\n"));
		return FALSE;
	}

	return TRUE;
} /* end xml_doc_sanity_checks */

static URLType
get_urltype_from_string (char* tmpbuf) {

	URLType rv;

	if (tmpbuf[0] == 'l' || tmpbuf[0] == 'L') {
		rv = PROTOCOL_LOCAL;
	}
	else if (tmpbuf[0] == 'h' || tmpbuf[0] == 'H') {
		rv = PROTOCOL_HTTP;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'F') {
		rv = PROTOCOL_FTP;
	}
	else {
		g_warning (_("Could not set URLType from config file!"));
	}
	return rv;
} /* end get_urltype_from_string */

static gboolean
get_boolean_value_from_string (char* tmpbuf) {

	gboolean rv;

	if (tmpbuf[0] == 't' || tmpbuf[0] == 'T') {
		rv = TRUE;
	}
	else if (tmpbuf[0] == 'f' || tmpbuf[0] == 'T') {
		rv = FALSE;
	}
	return rv;
} /* end get_boolean_value_from_string */

InstallOptions*
init_default_install_configuration (const char* config_file) {

	InstallOptions* rv;
	xmlDocPtr doc;
	xmlNodePtr base;
	char* tmpbuf;

	g_return_val_if_fail (config_file != NULL, NULL);

	if (!g_file_exists (config_file)) {
		create_default_metadata (config_file);
	}

	doc = xmlParseFile (config_file);

	if (xml_doc_sanity_checks (doc)==FALSE) {
		return NULL;
	}
	
	base = doc->root;

	rv = g_new0 (InstallOptions, 1);

	tmpbuf = xml_get_value (base, "PROTOCOL");
	rv->protocol = get_urltype_from_string (tmpbuf);

	rv->pkg_list = g_strdup (xml_get_value (base, "PKG_LIST"));
	rv->transaction_dir = g_strdup (xml_get_value (base, "TRANSACTION_DIR"));	

	tmpbuf = xml_get_value (base, "VERBOSE");
	rv->mode_verbose = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "SILENT");
	rv->mode_silent = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "DEBUG");
	rv->mode_debug = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "DRY_RUN");
	rv->mode_test = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "FORCE");
	rv->mode_force = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "DEPEND");
	rv->mode_depend = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "UPDATE");
	rv->mode_update = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "UNINSTALL");
	rv->mode_uninstall = get_boolean_value_from_string (tmpbuf);

	tmpbuf = xml_get_value (base, "DOWNGRADE");
	rv->mode_downgrade = get_boolean_value_from_string (tmpbuf);

	g_free (tmpbuf);
	xmlFreeDoc (doc);
	
	return rv;
} /* end init_default_install_configuration */

TransferOptions*
init_default_transfer_configuration (const char* config_file) {

	TransferOptions* rv;
	xmlDocPtr doc;
	xmlNodePtr base;

	g_return_val_if_fail (config_file != NULL, NULL);

	doc = xmlParseFile (config_file);

	if (xml_doc_sanity_checks (doc)==FALSE) {
		return NULL;
	}
	
	base = doc->root;

	rv = g_new0 (TransferOptions, 1);
	rv->port_number = atoi (xml_get_value (base, "PORT"));
	rv->hostname = g_strdup (xml_get_value (base, "HOSTNAME"));
	rv->pkg_list_storage_path = g_strdup (xml_get_value (base, "PKG_LIST_STORAGE_PATH"));
	rv->rpm_storage_path = g_strdup (xml_get_value (base, "RPM_STORAGE_PATH"));
	rv->tmp_dir = g_strdup (xml_get_value (base, "TMP_DIR"));	
	rv->rpmrc_file = g_strdup (xml_get_value (base, "RPMRC_FILE"));

	xmlFreeDoc (doc);
	
	return rv;
} /* end init_default_transfer_configuration */


void 
transferoptions_destroy (TransferOptions *topts)
{
	g_return_if_fail (topts!=NULL);
	g_free (topts->hostname);
	topts->hostname = NULL;
	g_free (topts->pkg_list_storage_path);
	topts->pkg_list_storage_path = NULL;
	g_free (topts->rpm_storage_path);
	topts->rpm_storage_path = NULL;
	g_free (topts->tmp_dir);
	topts->tmp_dir = NULL;
	g_free (topts->rpmrc_file);
	topts->rpmrc_file = NULL;
}

void 
installoptions_destroy (InstallOptions *iopts)
{
	g_return_if_fail (iopts!=NULL);
	g_free (iopts->pkg_list);
	g_free (iopts->transaction_dir);
	iopts->pkg_list = NULL;
	iopts->transaction_dir = NULL;
}
