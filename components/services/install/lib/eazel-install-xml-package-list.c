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

#include <gnome-xml/parser.h>

static PackageData* parse_package (xmlNode* package, gboolean set_toplevel);
static CategoryData* parse_category (xmlNode* cat);
static int parse_pkg_template (const char* pkg_template_file, char** result);


static PackageData*
parse_package (xmlNode* package, gboolean set_toplevel) {

	xmlNodePtr dep;
	PackageData* rv;

	rv = packagedata_new ();

	rv->name = g_strdup (xml_get_value (package, "NAME"));
	rv->version = g_strdup (xml_get_value (package, "VERSION"));
	rv->minor = g_strdup (xml_get_value (package, "MINOR"));
	rv->archtype = g_strdup (xml_get_value (package, "ARCH"));
	if (xml_get_value (package, "STATUS")) {
		rv->status = packagedata_status_str_to_enum (xml_get_value (package, "STATUS"));
	}
	if (xml_get_value (package, "MODSTATUS")) {
		rv->modify_status = packagedata_modstatus_str_to_enum (xml_get_value (package, "MODSTATUS"));
	}

	{
		char *tmp;
		tmp = xml_get_value (package, "BYTESIZE");
		if (tmp) {
			rv->bytesize = atoi (tmp);
		} else {
			rv->bytesize = 0;
		}
	}
	rv->summary = g_strdup (xml_get_value (package, "SUMMARY"));
	rv->distribution = trilobite_get_distribution ();
	if (set_toplevel) {
		rv->toplevel = TRUE;
	}
	
	/* Dependency Lists */
	rv->soft_depends = NULL;
	rv->hard_depends = NULL;
	rv->breaks = NULL;
	rv->modifies = NULL;

	dep = package->childs;
	while (dep) {
		if (g_strcasecmp (dep->name, "SOFT_DEPEND") == 0) {
			PackageData* depend;

			depend = parse_package (dep, FALSE);
			rv->soft_depends = g_list_append (rv->soft_depends, depend);
		}
		else if (g_strcasecmp (dep->name, "HARD_DEPEND") == 0) {
			PackageData* depend;

			depend = parse_package (dep, FALSE);
			rv->hard_depends = g_list_append (rv->hard_depends, depend);
		} else if (g_strcasecmp (dep->name, "BREAKS") == 0) {
			PackageData* depend;

			depend = parse_package (dep, FALSE);
			rv->breaks = g_list_append (rv->breaks, depend);
		} else if (g_strcasecmp (dep->name, "MODIFIES") == 0) {
			PackageData* depend;

			depend = parse_package (dep, FALSE);
			rv->modifies = g_list_append (rv->modifies, depend);
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
		g_print (_("*** No package nodes! ***\n"));
		g_free (category);
		g_error (_("*** Bailing from package parse! ***\n"));
	}
	while (pkg) {
		PackageData* pakdat;

		pakdat = parse_package (pkg, TRUE);
		category->packages = g_list_append (category->packages, pakdat);
		pkg = pkg->next;
	}

	return category;

} /* end parse_category */

GList* parse_shared (xmlDocPtr doc) 
{
	xmlNodePtr base;
	xmlNodePtr category;
	GList* rv;

	rv = NULL;
	base = doc->root;
	if (base == NULL) {
		xmlFreeDoc (doc);
		g_warning (_("*** The pkg list file contains no data! ***\n"));
		return NULL;
	}
	
	if (g_strcasecmp (base->name, "CATEGORIES")) {
		g_print (_("*** Cannot find the CATEGORIES xmlnode! ***\n"));
		xmlFreeDoc (doc);
		g_warning (_("*** Bailing from categories parse! ***\n"));
		return NULL;
	}
	
	category = doc->root->childs;
	if(category == NULL) {
		g_print (_("*** No Categories! ***\n"));
		xmlFreeDoc (doc);
		g_warning (_("*** Bailing from category parse! ***\n"));
		return NULL;
	}
	
	while (category) {
		CategoryData* catdat;

		catdat = parse_category (category);
		rv = g_list_append (rv, catdat);
		category = category->next;
	}

	xmlFreeDoc (doc);
	return rv;
}

GList* parse_memory_xml_package_list (const char *mem, int size) {
	xmlDocPtr doc;

	g_return_val_if_fail (mem!=NULL, NULL);

	doc = xmlParseMemory ((char*)mem, size);

	if (doc == NULL) {
		xmlFreeDoc (doc);
		return NULL;
	}
	
	return parse_shared (doc);
}

GList* 
parse_memory_transaction_file (const char *mem, 
			       int size)
{
	xmlDocPtr doc;
	xmlNodePtr base;
	xmlNodePtr packages;
	GList* rv;

	g_return_val_if_fail (mem!=NULL, NULL);

	doc = xmlParseMemory ((char*)mem, size);
	rv = NULL;
	if (doc == NULL) {
		xmlFreeDoc (doc);
		return NULL;
	}

	base = doc->root;
	if (g_strcasecmp (base->name, "TRANSACTION")) {
		g_print (_("*** Cannot find the TRANSACTION xmlnode! ***\n"));
		xmlFreeDoc (doc);
		g_warning (_("*** Bailing from transaction parse! ***\n"));
		return NULL;
	}
	
	packages = doc->root->childs->childs;
	if(packages == NULL) {
		g_print (_("*** No packages! ***\n"));
		xmlFreeDoc (doc);
		g_warning (_("*** Bailing from transaction parse! ***\n"));
		return NULL;
	}

	while (packages) {
		PackageData *pack;
		pack = parse_package (packages, TRUE);
		g_message ("TRANS PARSE %s", pack->name);
		rv = g_list_append (rv, pack);
		packages = packages->next;
	}	
}

GList*
parse_local_xml_package_list (const char* pkg_list_file) {
	xmlDocPtr doc;

	g_return_val_if_fail (pkg_list_file!=NULL, NULL);

	doc = xmlParseFile (pkg_list_file);
	
	if (doc == NULL) {
		fprintf (stderr, "***Unable to open pkg list file %s\n", pkg_list_file);
		xmlFreeDoc (doc);
		return NULL;
	}

	return parse_shared (doc);
	
} /*end fetch_xml_packages_local */

gboolean
generate_xml_package_list (const char* pkg_template_file,
                           const char* target_file) {

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
	char* retbuf;
	int index;
	char** entry_array;
	char** package_array;
	char *tags[] = {"NAME", "VERSION", "MINOR", "ARCH", "BYTESIZE", "SUMMARY", NULL};
	int num_tags = 5;
	int lines;
	
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

		package_array = g_strsplit (entry_array[index], ":", num_tags+1);

		if (package_array && package_array[0]) {
			xmlNodePtr packages;
			xmlNodePtr category;
			xmlNodePtr package;
			xmlNodePtr data;
			int i;

			if ((doc->root->childs == NULL) ||
			    (xmlGetProp (doc->root->childs, package_array[0]))) {
				category = xmlNewChild (doc->root, NULL, "CATEGORY", NULL);
				xmlSetProp (category, "name", package_array[0]);
				packages = xmlNewChild (category, NULL, "PACKAGES", NULL);
			}

			package = xmlNewChild (packages, NULL, "PACKAGE", NULL);
			
			for (i = 0; i <= num_tags; i++) {
				if (package_array[i+1]) {
					data = xmlNewChild (package, NULL, tags[i], package_array[i+1]);
				} else {
					g_warning ("line %d, tag %d (%s) is missing", index+1, i+1, tags[i]);
				}
			}
			g_strfreev (package_array);
		}
	}
	
	if (doc == NULL) {
		g_warning (_("*** Error generating xml package list! ***\n"));
		xmlFreeDoc (doc);
		return FALSE;
	}

	/* Check for existing file and if, rename, trying to find a .x
	   extension (x being a number) that isn't taken */
	if (g_file_exists (target_file)) {
		char *new_name;
		int c;
		c = 0;
		new_name = NULL;
		do {
			g_free (new_name);
			c++;
			new_name = g_strdup_printf ("%s.%d", target_file, c);
		} while (g_file_exists (new_name));		
		rename (target_file, new_name);
		g_free (new_name);
	}
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
		g_warning (_("*** Error reading package list! ***\n"));
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
} /* end parse_pkg_template */

/* Creates and returns an xmlnode for a packagedata struct.
   If given a droot and a title, uses that so create a subnode
   (primarily used for the recursiveness of PackageData objects.
   If not, creates a node with the name "PACKAGE" */
xmlNodePtr
eazel_install_packagedata_to_xml (const PackageData *pack, char *title, xmlNodePtr droot)
{
	xmlNodePtr root, node;
	char *tmp;
	GList *iterator;

	if (droot) {
		g_assert (title != NULL);
		root = xmlNewChild (droot, NULL, title, NULL);
	} else {
		root = xmlNewNode (NULL, "PACKAGE");
	}
	node = xmlNewChild (root, NULL, "NAME", pack->name);
	node = xmlNewChild (root, NULL, "VERSION", pack->version);
	node = xmlNewChild (root, NULL, "MINOR", pack->minor);
	node = xmlNewChild (root, NULL, "ARCH", pack->archtype);
	node = xmlNewChild (root, NULL, "SUMMARY", pack->summary);
	node = xmlNewChild (root, NULL, "STATUS", packagedata_status_enum_to_str (pack->status));
	node = xmlNewChild (root, NULL, "MODSTATUS", packagedata_modstatus_enum_to_str (pack->modify_status));

	tmp = trilobite_get_distribution_name(pack->distribution, FALSE, FALSE);
	node = xmlNewChild (root, NULL, "DISTRIBUTION", tmp);
	g_free (tmp);
	if (pack->distribution.version_major!=-1) {
		tmp = g_strdup_printf ("%d", pack->distribution.version_major);
		xmlSetProp (node, "major", tmp);
		g_free (tmp);
	}
	if (pack->distribution.version_minor!=-1) {
		tmp = g_strdup_printf ("%d", pack->distribution.version_minor);
		xmlSetProp (node, "minor", tmp);
		g_free (tmp);
	}

	tmp = g_strdup_printf ("%d", pack->bytesize);
	node = xmlNewChild (root, NULL, "BYTESIZE", tmp);
	g_free (tmp);

	for (iterator = pack->soft_depends; iterator; iterator = iterator->next) {
		eazel_install_packagedata_to_xml ((PackageData*)iterator->data, "SOFT_DEPEND", root);
	}
	for (iterator = pack->hard_depends; iterator; iterator = iterator->next) {
		node = xmlNewChild (root, NULL, "HARD_DEPEND", NULL);
		eazel_install_packagedata_to_xml ((PackageData*)iterator->data, "HARD_DEPEND", root);
	}
	for (iterator = pack->breaks; iterator; iterator = iterator->next) {
		eazel_install_packagedata_to_xml ((PackageData*)iterator->data, "BREAKS", root);
	}
	for (iterator = pack->modifies; iterator; iterator = iterator->next) {
		eazel_install_packagedata_to_xml ((PackageData*)iterator->data, "MODIFIES", root);
	}

	return root;
}

xmlNodePtr
eazel_install_packagelist_to_xml (GList *packages) {
	xmlNodePtr node;
	GList *iterator;

	node = xmlNewNode (NULL, "PACKAGES");
	for (iterator = packages; iterator; iterator = iterator->next) {
		xmlAddChild (node, 
			     eazel_install_packagedata_to_xml ((PackageData*)iterator->data, 
							       NULL, NULL));
	}

	return node;	
};

xmlNodePtr
eazel_install_categorydata_to_xml (const CategoryData *category)
{
	xmlNodePtr node;

	node = xmlNewNode (NULL, "CATEGORY");
	xmlSetProp (node, "name", category->name);
	xmlAddChild (node, eazel_install_packagelist_to_xml (category->packages));

	return node;
}




