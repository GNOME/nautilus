/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Ian McKellar <ian@eazel.com>
 *
 * The code responsible for the client inventory manipulation.
 *
 */

#include <config.h>

#include "eazel-inventory-utils.h"
#include "eazel-inventory-collect-software.h"

#include <gdk/gdk.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <gnome.h>
#include <gnome-xml/entities.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>

#include <libtrilobite/trilobite-md5-tools.h>
#include <libtrilobite/trilobite-core-distribution.h>

#define DEBUG_pepper		1

#define DIGEST_GCONF_PATH	"/apps/eazel-trilobite/inventory-digest"
#define DIGEST_GCONF_KEY	"inventory_digest_value"

/* eazel package system method to query the rpmdb for
 * all packages and return a GList of PackageData structs.
 */

static GList *
get_package_list (EazelPackageSystem *package_system)
{
	GList		*packages;

	packages = NULL;

	packages = eazel_package_system_query	(package_system,
					 	 NULL,
						 "",
						 EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR,
						 0);

	return packages;
}

/* add package data from the package database to the passed in xml document */
static xmlNodePtr
eazel_inventory_collect_packages (void) {
	char			*package_count_str;
	xmlNodePtr		packages_node;
	xmlNodePtr		current_package_node;
 	int			package_count;
	EazelPackageSystem	*package_system;
	GList			*packages;
	GList			*iterator;
	PackageData		*package;


    	/* add the PACKAGES node */
	packages_node = xmlNewNode (NULL, "PACKAGES");

	package_count = 0;

	package_system = eazel_package_system_new (NULL);

	packages = get_package_list (package_system);

	/* iterate through all of the installed packages */

	for (iterator = packages; iterator != NULL; iterator = g_list_next (iterator)) {

		package = (PackageData*) iterator->data;

 		/* add a node for this package */
        
		current_package_node = xmlNewChild (packages_node, NULL, "PACKAGE", NULL);
		package_count += 1;

		xmlSetProp (current_package_node, "name", package->name);
		xmlSetProp (current_package_node, "version", package->version);
		xmlSetProp (current_package_node, "release", package->minor);
		xmlSetProp (current_package_node, "epoch", package->archtype);

		gtk_object_unref (GTK_OBJECT (package));

	  }
    
    	/* update the count */
    
    	package_count_str = g_strdup_printf ("%d", package_count);
	xmlSetProp (packages_node, "count", package_count_str);
    
	/* clean up*/   
	gtk_object_unref (GTK_OBJECT (package_system));
	g_list_free (packages);

	return packages_node;

}

xmlNodePtr
eazel_inventory_collect_software (void) {
	DistributionInfo	distro;
	char			*distro_string;
	xmlNodePtr              node;
	
	node = xmlNewNode (NULL, "SOFTWARE");

	distro = trilobite_get_distribution ();
	distro_string = trilobite_get_distribution_name (distro, TRUE, FALSE);
	if (!distro_string) {
		/* do we really want to pass this string? */
		distro_string = g_strdup_printf ("Unknown Distribution");
	}
	xmlNewChild (node, NULL, "DISTRIBUTION", distro_string);
	g_free (distro_string);

	/* add the package info */
	xmlAddChild (node, eazel_inventory_collect_packages ()); 

	return node;
}

