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
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include <config.h>
#include "eazel-install-xml-package-list.h"

static PackageData* parse_package (xmlNode* package);
static CategoryData* parse_category (xmlNode* cat);
static int parse_pkg_template (const char* pkg_template_file, char **result);


static PackageData*
parse_package (xmlNode* package) {

	xmlNodePtr dep;
	PackageData* rv;

	rv = g_new0 (PackageData, 1);

	rv->name = g_strdup (xml_get_value (package, "NAME"));
	rv->version = g_strdup (xml_get_value (package, "VERSION"));
	rv->minor = g_strdup (xml_get_value (package, "MINOR"));
	rv->archtype = g_strdup (xml_get_value (package, "ARCH"));
	rv->bytesize = atoi (xml_get_value (package, "BYTESIZE"));
	rv->summary = g_strdup (xml_get_value (package, "SUMMARY"));
	
	/* Dependency Lists */
	rv->soft_depends = NULL;
	rv->hard_depends = NULL;

	dep = package->childs;
	while (dep) {
		if (g_strcasecmp (dep->name, "SOFT_DEPEND") == 0) {
			PackageData* depend;

			depend = parse_package (dep);
			rv->soft_depends = g_list_append (rv->soft_depends, depend);
		}
		else if (g_strcasecmp (dep->name, "HARD_DEPEND") == 0) {
			PackageData* depend;

			depend = parse_package (dep);
			rv->hard_depends = g_list_append (rv->hard_depends, depend);
		}

		dep = dep->next;

	}

	return rv;

} /* end parse package */

static CategoryData*
parse_category (xmlNode* cat) {

	CategoryData* category;
	xmlNode* pkg;

	category = g_new0 (CategoryData, 1);
	category->name = xmlGetProp (cat, "name");

	pkg = cat->childs->childs;
	if (pkg == NULL) {
		fprintf (stderr, "***No package nodes!***\n");
		g_free (category);
		g_error ("***Bailing from package parse!***\n");
	}
	while (pkg) {
		PackageData* pakdat;

		pakdat = parse_package (pkg);
		category->packages = g_list_append (category->packages, pakdat);
		pkg = pkg->next;
	}

	return category;

} /* end parse_category */

GList*
parse_local_xml_package_list (const char* pkg_list_file) {
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
		GList* t = c->packages;

		while (t) {
			PackageData* pack = t->data;
			GList* temp;

			temp = pack->soft_depends;
			while (temp) {
				g_free (temp->data);
				temp = temp->next;
			}
			g_list_free(pack->soft_depends);

			temp = pack->hard_depends;
			while (temp) {
				g_free (temp->data);
				temp = temp->next;
			}
			g_list_free (pack->hard_depends);
			
			g_free (t->data);

			t = t->next;
		}

		g_list_free (c->packages);
		g_free (c);

		categories = categories->next;
	}
	g_list_free (categories);

} /* end free_categories */

gboolean
generate_xml_package_list (const char* pkg_template_file, const char* target_file) {

/* This function will accept a colon delimited list of packages and generate
 * an xml package list for eazel-install.  The pkg_template_function should
 * be of the following format:
 * 
 * category name : package name : version : minor : archtype : bytesize : summary
 * 
 * Example:
 * 
 *
 * Essential Packages:anaconda:7.0:1:i386:2722261:The redhat installer
 * 
 */

	xmlDocPtr doc;
	xmlNodePtr category;
	xmlNodePtr packages;
	xmlNodePtr package;
	xmlNodePtr data;
	char* retbuf;
	int index;
	char** entry_array;
	char** package_array;
	gint lines;
	
	doc = xmlNewDoc ("1.0");
	doc->root = xmlNewDocNode (doc, NULL, "CATEGORIES", NULL);
	
	lines = parse_pkg_template (pkg_template_file, &retbuf);

	if (lines) {
		entry_array = g_strsplit (retbuf, "\n", lines);
	}

	for (index = 0; index < lines; index++) {

		if (entry_array[index] == NULL) {
			break;
		}

		g_print ("entry = %s\n", entry_array[index]);
		
		package_array = g_strsplit (entry_array[index], ":", 7);

		g_print ("pe0 = %s\n", package_array[0]);
		g_print ("pe1 = %s\n", package_array[1]);
		g_print ("pe2 = %s\n", package_array[2]);
		g_print ("pe3 = %s\n", package_array[3]);
		g_print ("pe4 = %s\n", package_array[4]);
		g_print ("pe5 = %s\n", package_array[5]);
		g_print ("pe6 = %s\n", package_array[6]);

 /* FIXME this has no error control right now.  It needs to be improved alot.  */
 
		if ((doc->root->childs == NULL) ||
		   (xmlGetProp (doc->root->childs, package_array[0]))) {
			category = xmlNewChild (doc->root, NULL, "CATEGORY", NULL);
			xmlSetProp (category, "name", package_array[0]);
			packages = xmlNewChild (category, NULL, "PACKAGES", NULL);
		}
		package = xmlNewChild (packages, NULL, "PACKAGE", NULL);
		data = xmlNewChild (package, NULL, "NAME", package_array[1]);
		data = xmlNewChild (package, NULL, "VERSION", package_array[2]);
		data = xmlNewChild (package, NULL, "MINOR", package_array[3]);
		data = xmlNewChild (package, NULL, "ARCH", package_array[4]);
		data = xmlNewChild (package, NULL, "BYTESIZE", package_array[5]);
		data = xmlNewChild (package, NULL, "SUMMARY", package_array[6]);
	}

	if (doc == NULL) {
		fprintf (stderr, "***Error generating xml package list !***\n");
		xmlFreeDoc (doc);
		return FALSE;
	}

	/* FIXME this should check to see if target_file already exists and save a copy. */

	xmlSaveFile (target_file, doc);
	xmlFreeDoc (doc);
	return TRUE;

} /* end generate_xml_package_list */

/**
   This just opens the specified file, read it and returns the number of lines
   and reads the contents into "result".
 */
static int
parse_pkg_template (const char* pkg_template_file, char **result) {

	FILE* input_file;
	char buffer[256];
	GString* string_data;
	int lines_read;
	
	g_assert (result!=NULL);

	string_data = g_string_new("");
	(*result) = NULL;
	lines_read = 0;

	input_file = fopen (pkg_template_file, "r");

	if (input_file == NULL) {
		fprintf (stderr, "***Error reading package list !***\n");
		return 0;
	}

	while (fgets (buffer, 255, input_file) != NULL) {
		lines_read++;
		g_string_append (string_data, buffer);
	}

	fclose (input_file);

	(*result) = g_strdup (string_data->str);
	g_string_free (string_data, TRUE);

	return lines_read;
}
