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
 * Author: Andy Hertzfeld <andy@eazel.com>
 *         J Shane Culpepper <pepper@eazel.com>
 *
 * The code responsible for the client inventory manipulation.
 *
 */

#include <config.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <rpm/rpmlib.h>
#include <gnome.h>
#include <gnome-xml/entities.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>
#include <libnautilus-extensions/nautilus-string.h>
#include "eazel-inventory-shared.h"

void	add_package_info	(xmlDocPtr 	configuration_metafile);
void	add_hardware_info	(xmlDocPtr 	configuration_metafile);
char	*read_proc_info		(const char	*proc_filename);
void	add_info		(xmlNodePtr 	node_ptr,
				 char		*data,
				 const char	*tag,
				 const char	*field_name);
void	add_io_info		(xmlNodePtr	node_ptr,
				 char		*io_data);

/* add package data from the package database to the passed in xml document */

void
add_package_info (xmlDocPtr	configuration_metafile) {

	char		package_count_str[32];
	char		*package_name;
	char		*package_version;
	char		*package_release;
	char		*version_str;
	xmlNodePtr	packages_node;
	xmlNodePtr	current_package_node;
 	rpmdb		rpm_db;
 	int		current_offset;
	int		rpm_result;
 	int		package_count;

	package_count = 0;
    
	/* open the rpm database for package lookups */
 
 	rpmReadConfigFiles (NULL, NULL);   
	rpm_result = rpmdbOpen ("", &rpm_db, O_RDONLY, 0644);
	if (rpm_result != 0) {
		g_message ("couldn't open package database: %d", rpm_result);
		return;
	}   
 	
    	/* add the PACKAGES node */
	packages_node = xmlNewChild (configuration_metafile->root, NULL, "PACKAGES", NULL);
    
	/* iterate through all of the installed packages */
    
	current_offset = rpmdbFirstRecNum (rpm_db);
 	while (current_offset)
	  {
		Header current_package = rpmdbGetRecord (rpm_db, current_offset);
        
        	headerGetEntry (current_package, RPMTAG_NAME, NULL, (void **) &package_name, NULL);
        	headerGetEntry (current_package, RPMTAG_VERSION, NULL, (void **) &package_version, NULL);
        	headerGetEntry(current_package, RPMTAG_RELEASE, NULL, (void **) &package_release, NULL);
        
 		/* add a node for this package */
        
		current_package_node = xmlNewChild (packages_node, NULL, "PACKAGE", NULL);
		package_count += 1;
        
		version_str = g_strdup_printf ("%s-%s", package_version, package_release);
		xmlSetProp (current_package_node, "name", package_name);
		xmlSetProp (current_package_node, "version", version_str);
       		g_free (version_str);
         
		headerFree (current_package);
		current_offset = rpmdbNextRecNum (rpm_db, current_offset);
	  	
		/* donate some time to gtk to handle updates, etc */
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
	  }
    
    	/* update the count */
    
    	sprintf (package_count_str, "%d", package_count);
	xmlSetProp (packages_node, "count", package_count_str);
    
	/* close the package data base */   
        rpmdbClose (rpm_db);    
}

/* utility routine to read a proc file into a string */

char*
read_proc_info	(const char	*proc_filename) {

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
	
	result = strdup (string_data->str);
	g_string_free (string_data, TRUE);
	g_free (path_name);

	return result;
}

/* utility routine to extract information from a string and add it to an XML node */

void
add_info (xmlNodePtr	node_ptr, char	*data, const char	*tag, const char	*field_name) {

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
		if (nautilus_str_has_prefix (info_array[index], field_name)) {
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

void
add_io_info (xmlNodePtr	node_ptr, char	*io_data) {

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

void
add_hardware_info (xmlDocPtr	configuration_metafile) {

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
	g_free (temp_string);	

	/* now handle IO port info */
	this_node = xmlNewChild (hardware_node, NULL, "IOPORTS", NULL);
	temp_string = read_proc_info ("ioports");
	add_io_info (this_node, temp_string);
	g_free (temp_string);	
}

/* synchronize an existing metafile with the rpm database */
xmlDocPtr
synchronize_configuration_metafile () {

	return NULL;
}

/* update_package_metafile is called during initialization time to create or
   synchronize the packages metafile */

xmlDocPtr
update_configuration_metafile () {

	return NULL;
}

/* create the configuration metafile and add package and hardware configuration info to it */
xmlDocPtr
create_configuration_metafile (void) {

	/* create a new xml document */
	time_t		current_time;
	xmlNodePtr	container_node;
    	char		*time_string;
	char		host_name[512];
	xmlDocPtr	configuration_metafile;

	configuration_metafile = xmlNewDoc ("1.0");
	
	gethostname (&host_name[0], 511);
	container_node = xmlNewDocNode (configuration_metafile, NULL, "CONFIGURATION", NULL);
    
	configuration_metafile->root = container_node;
	time (&current_time);
	time_string = g_strdup (ctime (&current_time));
	time_string[strlen(time_string) - 1] = '\0';
	xmlSetProp (container_node, "computer", host_name);	
	xmlSetProp (container_node, "date", time_string);
	g_free (time_string);
	
	/* FIXME bugzilla.eazel.com 732: need to set up nautilus version here */

	/* add the package info */
	add_package_info (configuration_metafile);
	
	/* add the hardware info */
	add_hardware_info (configuration_metafile);
	
	return configuration_metafile;
}
