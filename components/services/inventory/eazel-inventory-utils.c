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
 *
 * The code responsible for the client inventory manipulation.
 *
 */

#include <config.h>

#include "eazel-inventory-utils.h"

#include <gdk/gdk.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <gnome.h>
#include <gnome-xml/entities.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>
#include <gconf/gconf.h>
#include <gconf/gconf-engine.h>

#include <libtrilobite/trilobite-md5-tools.h>
#include <libtrilobite/trilobite-core-distribution.h>

#define DEBUG_pepper		1

#define DIGEST_GCONF_PATH	"/apps/eazel-trilobite/inventory-digest"
#define DIGEST_GCONF_KEY	"inventory_digest_value"


static GConfEngine *conf_engine = NULL;

/* called by atexit to close the gconf connection */
static void
eazel_inventory_gconf_done (void)
{
	gconf_engine_unref (conf_engine);
}


/* make sure gconf is initialized */
static void
check_gconf_init (void)
{
	GError *error = NULL;

	if (! gconf_is_initialized ()) {
		char *argv[] = { "trilobite", NULL };

		if (! gconf_init (1, argv, &error)) {
			g_assert (error != NULL);
			g_warning ("gconf init error: %s", error->message);
                        g_error_free (error);
		}
	}
	if (conf_engine == NULL) {
		conf_engine = gconf_engine_get_default ();
		g_atexit (eazel_inventory_gconf_done);
	}
}



/* ripped straight out of libnautilus-extensions because we didn't want the
 * dependency for one small function
 */
static gboolean
str_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
	} while (*h++ == *n++);
	return FALSE;
}

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
static void
add_package_info (xmlDocPtr	configuration_metafile, xmlNodePtr	node)
{

	char			*package_count_str;
	xmlNodePtr		packages_node;
	xmlNodePtr		current_package_node;
 	int			package_count;
	EazelPackageSystem	*package_system;
	GList			*packages;
	GList			*iterator;
	PackageData		*package;
	char			*package_name;
	char			*package_version;
	char			*package_release;
	char			*package_arch;


	package_count = 0;

	package_system = eazel_package_system_new (NULL);

	packages = get_package_list (package_system);

    	/* add the PACKAGES node */
	packages_node = xmlNewChild (node, NULL, "PACKAGES", NULL);
    
	/* iterate through all of the installed packages */

	for (iterator = packages; iterator != NULL; iterator = g_list_next (iterator)) {

		package = (PackageData*) iterator->data;

		package_name = package->name;
		package_version = package->version;
		package_release = package->minor;
		package_arch = package->archtype;

 		/* add a node for this package */
        
		current_package_node = xmlNewChild (packages_node, NULL, "PACKAGE", NULL);
		package_count += 1;

		xmlSetProp (current_package_node, "name", package_name);
		xmlSetProp (current_package_node, "version", package_version);
		xmlSetProp (current_package_node, "release", package_release);
		xmlSetProp (current_package_node, "epoch", package_arch);

		gtk_object_unref (GTK_OBJECT (package));

	  }
    
    	/* update the count */
    
    	package_count_str = g_strdup_printf ("%d", package_count);
	xmlSetProp (packages_node, "count", package_count_str);
    
	/* clean up*/   
	gtk_object_unref (GTK_OBJECT (package_system));
	g_list_free (packages);

}

/* utility routine to read a proc file into a string */
static char*
read_proc_info	(const char	*proc_filename)
{

	FILE	*thisFile;
	char	*result;
	char	buffer[256];
	char	*path_name;
	GString	*string_data;
	
	path_name = g_strdup_printf ("/proc/%s", proc_filename);	
	string_data = g_string_new ("");
	thisFile = fopen (path_name, "r");
	
	while (fgets (buffer, 255, thisFile) != NULL) {
		g_string_append (string_data, buffer);		
	}
	fclose (thisFile);
	
	result = g_strdup (string_data->str);
	g_string_free (string_data, TRUE);
	g_free (path_name);

	return result;
}

/* utility routine to extract information from a string and add it to an XML node */
static void
add_info (xmlNodePtr	node_ptr, char	*data, const char	*tag, const char	*field_name)
{

	int	index;
	char	**info_array;
	char	*field_data;

	field_data = NULL;
	
	/* parse the data into a string array */
	info_array = g_strsplit (data, "\n", 32);
	/* iterate through the data isolating the field */
	for (index = 0; index < 32; index++) {
		if (info_array[index] == NULL)
			break;
		if (str_has_prefix (info_array[index], field_name)) {
			field_data = info_array[index] + strlen(field_name);
			field_data = strchr (field_data, ':') + 1;
			field_data = g_strchug (field_data);
			break;
		}
	}
	
	/* add the requested node if the field was found */
	if (field_data) {
		xmlNodePtr new_node;
		new_node = xmlNewChild (node_ptr, NULL, tag, NULL);
		xmlNodeSetContent (new_node, field_data);
	}
	g_strfreev (info_array);
}

/* utility routine to process io info */
static void
add_io_info (xmlNodePtr	node_ptr, char	*io_data)
{

	int		index;
	char		*temp_str;
	xmlNodePtr	new_node;
	char		**info_array;
	
	/* parse the data into a string array */
	info_array = g_strsplit (io_data, "\n", 64);
	/* iterate through the data creating a record for each line */
	for (index = 0; index < 64; index++) {
		if (info_array[index] == NULL)
			break;
		new_node = xmlNewChild (node_ptr, NULL, "IORANGE", NULL);
		temp_str = strchr (info_array[index], ':');
		if (temp_str) {
			*temp_str = '\0';
			xmlSetProp (new_node, "RANGE", g_strstrip (info_array[index]));
			xmlSetProp (new_node, "TYPE",  g_strstrip (temp_str + 1));
		}
		
	}
	
	g_strfreev (info_array);
}

/* add hardware info from the /proc directory to the passed in xml document */
static void
add_hardware_info (xmlDocPtr	configuration_metafile)
{

    	xmlNodePtr	cpu_node;
	xmlNodePtr	this_node;
	xmlNodePtr	hardware_node;
	char		*temp_string;
	
	/* add the HARDWARE node */
	hardware_node = xmlNewChild (configuration_metafile->root, NULL, "HARDWARE", NULL);
 	
	/* capture various information from the /proc filesystem */
	/* first, capture memory info */
	
	temp_string = read_proc_info ("meminfo");
	add_info (hardware_node, temp_string, "MEMORYSIZE", "MemTotal");
	add_info (hardware_node, temp_string, "SWAPSIZE", "SwapTotal");
	g_free (temp_string);	

	/* now handle CPU info */
	cpu_node = xmlNewChild (hardware_node, NULL, "CPU", NULL);
	temp_string = read_proc_info ("cpuinfo");
	add_info (cpu_node, temp_string, "TYPE", "processor");
	add_info (cpu_node, temp_string, "VENDOR", "vendor_id");
	add_info (cpu_node, temp_string, "FAMILY", "cpu family");
	add_info (cpu_node, temp_string, "MODEL", "model");
	add_info (cpu_node, temp_string, "MODELNAME", "model name");
	add_info (cpu_node, temp_string, "STEPPING", "stepping");
	add_info (cpu_node, temp_string, "SPEED", "cpu MHz");
	add_info (cpu_node, temp_string, "CACHE", "cache size");
	add_info (cpu_node, temp_string, "BOGOMIPS", "bogomips");
	add_info (cpu_node, temp_string, "FLAGS", "flags");
	g_free (temp_string);	

	/* now handle IO port info */
	this_node = xmlNewChild (hardware_node, NULL, "IOPORTS", NULL);
	temp_string = read_proc_info ("ioports");
	add_io_info (this_node, temp_string);
	g_free (temp_string);	
}

/* add hardware info from the /proc directory to the passed in xml document */
static void
add_software_info (xmlDocPtr	configuration_metafile)
{

	xmlNodePtr		software_node;
    	xmlNodePtr		distribution_node;
	DistributionInfo	distro;
	char			*distro_string;
	
	/* add the SOFTWARE node */
	software_node = xmlNewChild (configuration_metafile->root, NULL, "SOFTWARE", NULL);

	/* add the distribution string */
	distro = trilobite_get_distribution ();
	distro_string = trilobite_get_distribution_name (distro, TRUE, FALSE);
	distribution_node = xmlNewChild (software_node, NULL, "DISTRIBUTION", NULL);
	if (!distro_string) {
		distro_string = g_strdup_printf ("Unknown Distribution");
	}
	xmlNodeSetContent (distribution_node, distro_string);
	g_free (distro_string);

	/* add the package info */
	add_package_info (configuration_metafile, software_node);

}

/* create the configuration metafile and add package and hardware configuration info to it */
static xmlDocPtr
eazel_create_configuration_metafile (void)
{

	/* create a new xml document */
	time_t		current_time;
	xmlNodePtr	container_node;
    	char		*time_string;
	char		*host_name;
	xmlDocPtr	configuration_metafile;

	configuration_metafile = xmlNewDoc ("1.0");
	
	check_gconf_init ();
	host_name = gconf_engine_get_string (conf_engine, KEY_GCONF_EAZEL_INVENTORY_MACHINE_NAME, NULL);
	if (host_name == NULL) {
		host_name = g_strdup ("");
	}

	container_node = xmlNewDocNode (configuration_metafile, NULL, "CONFIGURATION", NULL);
    
	configuration_metafile->root = container_node;
	time (&current_time);
	time_string = g_strdup (ctime (&current_time));
	time_string[strlen(time_string) - 1] = '\0';
	xmlSetProp (container_node, "computer", host_name);	
	g_free (host_name);

/*	xmlSetProp (container_node, "date", time_string); */
	g_free (time_string);
	
	/* add the software info */
	add_software_info (configuration_metafile);
	
	/* add the hardware info */
	add_hardware_info (configuration_metafile);
	
	return configuration_metafile;
}


/* get last digest value stored in gconf */
static char *
get_digest_from_gconf (const char *key)
{
	GError	*error;
	char	*full_key;
	char	*helper;
	char	*value;

	error = NULL;
	value = NULL;

	check_gconf_init ();
	full_key = g_strdup_printf ("%s/%s", DIGEST_GCONF_PATH, key);

	/* convert all spaces to underscores */
	while ((helper = strchr (full_key, ' ')) != NULL) {
		*helper = '_';
	}

	value = gconf_engine_get_string (conf_engine, full_key, &error);
	if (error != NULL) {
		g_warning ("inventory cannot find key: '%s': %s", full_key, error->message);
		g_error_free (error);
	}
	g_free (full_key);
	return value;
}


/* The external function that will update the gconf digest value when
 * a successful inventory upload occurs.
 */

gboolean
update_gconf_inventory_digest (unsigned char value[16])
{
	GError *error;
	char *full_key;
	char *helper;
	const char *digest_string;
	gboolean return_val;

	return_val = TRUE;

	error = NULL;

	digest_string = trilobite_md5_get_string_from_md5_digest (value);
	check_gconf_init ();
	full_key = g_strdup_printf ("%s/%s", DIGEST_GCONF_PATH, DIGEST_GCONF_KEY);

	/* convert all spaces to underscores */
	while ((helper = strchr (full_key, ' ')) != NULL) {
		*helper = '_';
	}

#ifdef DEBUG_pepper
	g_print ("Key: %s Digest: %s\n", full_key, digest_string);
#endif

	gconf_engine_unset (conf_engine, full_key, &error);
	if (error != NULL) {
		g_warning ("Eazel Inventory: gconf could not delete key '%s' : %s",
			   full_key, error->message);
		g_error_free (error);
	}

	gconf_engine_set_string (conf_engine, full_key, digest_string, &error);
	if (error != NULL) {
		g_warning ("inventory cannot add gconf key: '%s': %s", full_key, error->message);
		g_error_free (error);
		return_val = FALSE;
	}
	g_free (full_key);

	return return_val;
}

/* The main external api for inventory gathering.  It will
 * generate the inventory.xml and check to see if the last
 * uploaded inventory is different.  If the checksum differs
 * and xml file was generated successfully , the function
 * returns TRUE, otherwise FALSE.
 */
gboolean
eazel_gather_inventory (void)
{
	char		*inventory_file_name;
	xmlDocPtr	inventory_doc;
	unsigned char	md5_digest[16];
	unsigned char	old_digest[16];
	char		*digest_string;
	gboolean	return_val;
#ifdef DEBUG_pepper
	const char	*digest_test;
#endif

	return_val = FALSE;

	inventory_doc = eazel_create_configuration_metafile ();
	if (inventory_doc == NULL) {
		g_warning ("There was an error gathering your inventory");
	}

	/* save the configuration file */
	inventory_file_name = eazel_inventory_local_path();
	xmlSaveFile (inventory_file_name, inventory_doc);
	xmlFreeDoc (inventory_doc);

	/* generate a md5 digest of the new file and compare it to the last upload */

	trilobite_md5_get_digest_from_file (inventory_file_name, md5_digest);
#ifdef DEBUG_pepper
	digest_test = trilobite_md5_get_string_from_md5_digest (md5_digest);
	g_print ("File: %s Digest: %s\n", inventory_file_name, digest_test);
#endif

	digest_string = get_digest_from_gconf (DIGEST_GCONF_KEY);
	if (digest_string != NULL) {
		trilobite_md5_get_digest_from_md5_string (digest_string, old_digest);
#ifdef DEBUG_pepper
		g_print ("Old GConf Digest value: %s\n", digest_string);
#endif

		if (memcmp (old_digest, md5_digest, 16) != 0) {
			return_val = TRUE;
		}
	} else {
		return_val = TRUE;
	}

	g_free (inventory_file_name);
	g_free (digest_string);

	return return_val;

}

/* return the local path to store and retrieve the inventory XML from */
gchar *eazel_inventory_local_path			(void) {
	return g_strdup_printf ("%s/.nautilus/configuration.xml", g_get_home_dir ());
}

void eazel_inventory_update_md5 () {
	unsigned char	md5_digest[16];
	char *inventory_file_name;

	inventory_file_name = eazel_inventory_local_path();
	trilobite_md5_get_digest_from_file (inventory_file_name, md5_digest);
	if (!update_gconf_inventory_digest (md5_digest)) {
		g_print ("failed to update digest\n");
	}
	g_free (inventory_file_name);

}
