/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2001 Eazel, Inc.
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
 * Authors: Ian McKellar <ian@eazel.com>
 *          J Shane Culpepper <pepper@eazel.com>
 *
 * The code responsible for the client inventory manipulation.
 *
 */

#include <config.h>

#include "eazel-inventory-collect-hardware.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <glib.h>
#include <string.h>
#include <gnome-xml/tree.h>

#define PCI_DEVICES_FILE   "/proc/bus/pci/devices"

#define IDE_BUS_MAX        8
#define IDE_BUS_FILE       "/proc/ide/ide%d/channel"
#define IDE_DRIVE_MIN      'a'
#define IDE_DRIVE_MAX      'z' /* FIXME: is this right? */
#define IDE_DRIVE_DIR      "/proc/ide/ide%d/hd%c/"
#define IDE_DRIVE_MEDIA    IDE_DRIVE_DIR "media"
#define IDE_DRIVE_CACHE    IDE_DRIVE_DIR "cache"
#define IDE_DRIVE_CAPACITY IDE_DRIVE_DIR "capacity"
#define IDE_DRIVE_MODEL    IDE_DRIVE_DIR "model"

#define MAX_LINE_LEN 4096

static void
add_device_property (xmlNodePtr device_node, const char *key, 
		const char *value) {
	xmlNodePtr property;

	property = xmlNewChild (device_node, NULL, "PROPERTY", value);
	xmlSetProp (property, "NAME", key);
}

/* collect information about devices on the PCI bus */
static xmlNodePtr
eazel_inventory_collect_pci (void) {
	FILE *pci_file;
	char bus_number[3], devfn[3], vendor[5], device[5];
	char line[MAX_LINE_LEN+1];
	xmlNodePtr bus_node, device_node;

	pci_file = fopen (PCI_DEVICES_FILE, "r");

	if (pci_file == NULL) {
		return NULL;
	}

	bus_node = xmlNewNode (NULL, "BUS");
	xmlSetProp (bus_node, "TYPE", "PCI");

	while (!feof (pci_file) && !ferror (pci_file)) {
		fgets (line, MAX_LINE_LEN, pci_file);

		sscanf (line, "%2s%2s\t%4s%4s", bus_number, devfn, vendor,
				device);

		device_node = xmlNewChild (bus_node, NULL, "DEVICE", NULL);
		add_device_property (device_node, "Vendor-ID", vendor);
		add_device_property (device_node, "Device-ID", device);
		add_device_property (device_node, "DeviceFunction", devfn);
		add_device_property (device_node, "BusNumber", bus_number);
	}

	return bus_node;
}

static char *
ide_get_value (int bus, char drive, const char *key) {
	char *value_path;
	FILE *value_file;
	char line[MAX_LINE_LEN+1] = "";
	int i;

	value_path = g_strdup_printf (key, bus, drive);
	value_file = fopen (value_path, "r");
	g_free (value_path);

	if (value_file == NULL) {
		return NULL;
	}

	fgets (line, MAX_LINE_LEN, value_file);

	fclose (value_file);

	i = strlen (line);

	if (i == 0) {
		/* the file is empty */
		return NULL;
	}

	/* remove trailing whitespace */
	i--;
	while ( (i >= 0) && isspace (line[i])) {
		line[i] = '\0';
		i--;
	}

	return g_strdup (line);
}

/* collect information about devices on the IDE bus */
static xmlNodePtr
eazel_inventory_collect_ide (void) {
	FILE *ide_file;
	char *bus_path;
	int bus;
	char drive;
	char *string;
	xmlNodePtr bus_node, device_node;

	bus_node = xmlNewNode (NULL, "BUS");
	xmlSetProp (bus_node, "TYPE", "IDE");

	for (bus = 0; bus < IDE_BUS_MAX; bus++) {
		bus_path = g_strdup_printf (IDE_BUS_FILE, bus);
		ide_file = fopen (bus_path, "r");
		if (ide_file) {
			fclose (ide_file); /* this file isn't important */

			/* Do we want to indicate which bus each drive
			 * is on? Do we care? Can we imply this from the
			 * device names? Find out in the next exciting
			 * episode of "Trilobite and Friends".
			 */

			for (drive = IDE_DRIVE_MIN; drive <= IDE_DRIVE_MAX;
					drive++) {
				string = ide_get_value (bus, drive,
						IDE_DRIVE_MEDIA);

				if (string == NULL) {
					continue;
				}

				device_node = xmlNewChild (bus_node, NULL,
						"DEVICE", NULL);

				add_device_property (device_node, "Media",
						string);
				g_free (string);

				string = g_strdup_printf ("hd%c", drive);
				add_device_property (device_node, "Device",
						string);
				g_free (string);

				string = ide_get_value (bus, drive,
						IDE_DRIVE_MODEL);
				if (string) {
					add_device_property (device_node, 
							"Model", string);
					g_free (string);
				}

				string = ide_get_value (bus, drive,
						IDE_DRIVE_CAPACITY);
				if (string) {
					int tmp;

					tmp = atoi (string);
					tmp /= 2048;

					g_free (string);

					string = g_strdup_printf ("%d", tmp);

					add_device_property (device_node, 
							"Capacity", string);
					g_free (string);
				}

				string = ide_get_value (bus, drive,
						IDE_DRIVE_CACHE);
				if (string) {
					add_device_property (device_node, 
							"Cache", string);
					g_free (string);
				}
						
			}
		}
		g_free (bus_path);
	}

	return bus_node;
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

/* utility routine to extract information from a string and add it to an XML node */
static void
add_info (xmlNodePtr	node_ptr, 
	  char		*data, 
	  const char	*tag, 
	  const char	*field_name)
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
			field_data = info_array[index] + strlen (field_name);
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



/* utility routine to read a proc file into a string */
static char*
read_proc_info	(const char	*proc_filename)
{

	FILE	*thisFile;
	char	*result;
	char	buffer[MAX_LINE_LEN+1];
	char	*path_name;
	GString	*string_data;
	
	path_name = g_strdup_printf ("/proc/%s", proc_filename);	
	string_data = g_string_new ("");
	thisFile = fopen (path_name, "r");
	
	while (fgets (buffer, MAX_LINE_LEN, thisFile) != NULL) {
		g_string_append (string_data, buffer);		
	}
	fclose (thisFile);
	
	result = g_strdup (string_data->str);
	g_string_free (string_data, TRUE);
	g_free (path_name);

	return result;
}


static xmlNodePtr
eazel_inventory_collect_memory (void) {
	xmlNodePtr	this_node;
	char		*temp_string;

	this_node = xmlNewNode (NULL, "MEMORY");
	temp_string = read_proc_info ("meminfo");
	add_info (this_node, temp_string, "MEMORYSIZE", "MemTotal");
	add_info (this_node, temp_string, "SWAPSIZE", "SwapTotal");
	g_free (temp_string);	

	return this_node;
}

static xmlNodePtr
eazel_inventory_collect_cpu (void) {
	xmlNodePtr	cpu_node;
	char		*temp_string;

	cpu_node = xmlNewNode (NULL, "CPU");

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

	return cpu_node;
}

xmlNodePtr
eazel_inventory_collect_hardware (void) {
	xmlNodePtr node = xmlNewNode (NULL, "HARDWARE");

	xmlAddChild (node, eazel_inventory_collect_cpu ()); 
	xmlAddChild (node, eazel_inventory_collect_memory ()); 
	xmlAddChild (node, eazel_inventory_collect_pci ()); 
	xmlAddChild (node, eazel_inventory_collect_ide ()); 

	return node;
}
