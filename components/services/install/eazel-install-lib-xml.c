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

static PackageData* parse_package (xmlNode* package);
static CategoryData* parse_category (xmlNode* cat);

char*
xml_get_value (xmlNode* node, const char* name) {
	char* ret;
	xmlNode *child;

	ret = xmlGetProp (node, name);
	if (ret) {
		return ret;
	}
	child = node->childs;
	while (child) {
		if (g_strcasecmp (child->name, name) == 0) {
			ret = xmlNodeGetContent (child);
			if (ret) {
				return ret;
			}
		}
		child = child->next;
	}
	return NULL;
} /* end xml_get_value */


InstallOptions*
init_default_install_configuration (const char* config_file) {

	InstallOptions* rv;
	xmlDocPtr doc;
	xmlNodePtr base;
	char* tmpbuf;
	
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
	rv->rpmrc_file = g_strdup (xml_get_value (base, "RPMRC_FILE"));
	rv->pkg_list_file = g_strdup (xml_get_value (base, "PKG_LIST_FILE"));
	rv->rpm_storage_dir = g_strdup (xml_get_value (base, "RPM_STORAGE_DIR"));
	rv->install_tmpdir = g_strdup (xml_get_value (base, "TMPDIR"));	

	g_free (tmpbuf);
	xmlFreeDoc (doc);
	
	return rv;
}

static PackageData*
parse_package (xmlNode* package) {

	xmlNode* dep;
	PackageData* rv;
	char* tmp;

	rv = g_new0 (PackageData, 1);
	rv->rpm_name = g_strdup (xml_get_value (package, "RPM_NAME"));
	rv->name = g_strdup (xml_get_value (package, "NAME"));
	rv->summary = g_strdup (xml_get_value (package, "SUMMARY"));
	rv->version = g_strdup (xml_get_value (package, "VERSION"));
	tmp = xml_get_value (package, "SOURCE");
	if (tmp[0] == 't' || tmp[0] == 'T') {
		rv->srcfile = TRUE;
	}
	else if (tmp[0] == 'f' || tmp[0] == 'F') {
		rv->srcfile = FALSE;
	}
	
	rv->bytesize = atoi (xml_get_value (package, "BYTESIZE"));
	
	/* Dependency Lists */
	rv->SoftDepends = NULL;
	rv->HardDepends = NULL;

	dep = package->childs;
	while (dep) {
		if (g_strcasecmp (dep->name, "SOFT_DEPEND") == 0) {
			PackageData* depend;

			depend = parse_package (dep);
			rv->SoftDepends = g_list_append (rv->SoftDepends, depend);
		}
		else if (g_strcasecmp (dep->name, "HARD_DEPEND") == 0) {
			PackageData* depend;

			depend = parse_package (dep);
			rv->HardDepends = g_list_append (rv->HardDepends, depend);
		}

		dep = dep->next;

	}
	
	dump_package_list (rv);

	return rv;

} /* end parse package */

static CategoryData*
parse_category (xmlNode* cat) {

	CategoryData* category;
	xmlNode* pkg;

	category = g_new0 (CategoryData, 1);
	category->name = xmlGetProp (cat, "name");

	g_print ("Category Name = %s\n", category->name);
	
	pkg = cat->childs->childs;
	if (pkg == NULL) {
		fprintf (stderr, "***No package nodes!***\n");
		g_free (category);
		g_error ("***Bailing from package parse!***\n");
	}
	while (pkg) {
		PackageData* pakdat;

		pakdat = parse_package (pkg);
		category->Packages = g_list_append (category->Packages, pakdat);
		pkg = pkg->next;
	}

	return category;

} /* end parse_category */

GList*
fetch_xml_package_list_local (const char* pkg_list_file) {
	GList* rv;
	xmlDocPtr doc;
	xmlNodePtr base;
	xmlNodePtr category;

	rv = NULL;
		
	doc = xmlParseFile (pkg_list_file);
	
	if (doc == NULL) {
		fprintf (stderr, "***Unable to open pkg list file!***\n");
		xmlFreeDoc (doc);
		g_assert (doc != NULL);
	}

	base = doc->root;
	if (base == NULL) {
		fprintf (stderr, "***The pkg list file contains no data!***\n");
		xmlFreeDoc (doc);
		g_assert (base != NULL);
	}
	
	if (g_strcasecmp (base->name, "CATEGORIES")) {
		fprintf (stderr, "***Cannot find the CATEGORIES xmlnode!***\n");
		xmlFreeDoc (doc);
		g_error ("***Bailing from categories parse!***\n");
	}
	
	category = doc->root->childs;
	if(category == NULL) {
		fprintf (stderr, "***No Categories!***\n");
		xmlFreeDoc (doc);
		g_error ("***Bailing from category parse!***\n");
	}
	
	while (category) {
		CategoryData* catdat;

		catdat = parse_category (category);
		rv = g_list_append (rv, catdat);
		category = category->next;
	}

	xmlFreeDoc (doc);
	return rv;
	
} /*end fetch_xml_packages_local */

void
free_categories (GList* categories) {

	while (categories) {
		CategoryData* c = categories->data;
		GList* t = c->Packages;

		while (t) {
			PackageData* pack = t->data;
			GList* temp;

			temp = pack->SoftDepends;
			while (temp) {
				g_free (temp->data);
				temp = temp->next;
			}
			g_list_free(pack->SoftDepends);

			temp = pack->HardDepends;
			while (temp) {
				g_free (temp->data);
				temp = temp->next;
			}
			g_list_free (pack->HardDepends);
			
			g_free (t->data);

			t = t->next;
		}

		g_list_free (c->Packages);
		g_free (c);

		categories = categories->next;
	}
	g_list_free (categories);

} /* end free_categories */
