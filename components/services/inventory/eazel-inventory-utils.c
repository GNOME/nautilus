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
#include "eazel-inventory-collect-hardware.h"
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
	xmlAddChild (container_node, eazel_inventory_collect_software ());

	/* add the hardware info */
	xmlAddChild (container_node, eazel_inventory_collect_hardware ());
	
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
