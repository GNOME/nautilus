/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 *  trilobite-redirect: functions to fetch a redirection table from
 *  a remote xml file, store it in gconf, and then lookup entries
 *  later.  this may only be useful for eazel services.
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Robey Pointer <robey@eazel.com>
 */

#include <config.h>
#include "trilobite-redirect.h"

#include "trilobite-core-network.h"
#include "trilobite-core-utils.h"
#include "trilobite-file-utilities.h"
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf.h>
#include <gconf/gconf-engine.h>
#include <gnome-xml/parser.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static gboolean trilobite_redirect_parse_xml (char *blob, 
					      int   length);

#define REDIRECT_TABLE_URI	"eazel-services:/table.xml"
#define REDIRECT_GCONF_PATH	"/apps/eazel-trilobite/redirect-table"

#define SERVICES_DEFAULT_HOST	"services.eazel.com"
#define SERVICES_GCONF_PATH	"/apps/eazel-trilobite/services-host"

static GConfEngine *conf_engine = NULL;


/* called by atexit so we can close the gconf connection */
static void
trilobite_redirect_done (void)
{
	gconf_engine_unref (conf_engine);
}

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
		g_atexit (trilobite_redirect_done);
	}
}


/* erase everything currently in our gconf table (so we can add new entries now) */
static void
wipe_redirect_table (void)
{
	GSList *list, *iter;
	GError *error = NULL;
	GConfEntry *entry;

	check_gconf_init ();
	list = gconf_engine_all_entries (conf_engine, REDIRECT_GCONF_PATH, &error);
	if (error != NULL) {
		g_warning ("gconf wipe error: %s", error->message);
		g_error_free (error);
		return;
	}

	for (iter = list; iter; iter = g_slist_next (iter)) {
		entry = (GConfEntry *) (iter->data);
		gconf_engine_unset (conf_engine, gconf_entry_get_key (entry), &error);
		if (error != NULL) {
			g_warning ("trilobite redirect: gconf couldn't delete key '%s': %s",
				   gconf_entry_get_key (entry), error->message);
			g_error_free (error);
		}
		gconf_entry_free (entry);
	}
	g_slist_free (list);
}

static void
add_redirect (const char *key, const char *value)
{
	GError *error = NULL;
	char *full_key, *p;

	check_gconf_init ();
	full_key = g_strdup_printf ("%s/%s", REDIRECT_GCONF_PATH, key);

	/* convert all spaces to dashes */
	while ((p = strchr (full_key, ' ')) != NULL) {
		*p = '-';
	}

	gconf_engine_set_string (conf_engine, full_key, value, &error);
	if (error != NULL) {
		g_warning ("trilobite redirect: gconf can't add key '%s': %s", full_key, error->message);
		g_error_free (error);
	}
	g_free (full_key);
}

/* parse an xml file into the redirect table */
gboolean
trilobite_redirect_parse_xml (char *blob, int length)
{
	xmlDocPtr doc;
	xmlNodePtr base, child, child2;
	char *name, *uri;

	g_return_val_if_fail (blob != NULL, FALSE);
	g_return_val_if_fail (length > 0, FALSE);

	/* <rant> libxml will have a temper tantrum if there is whitespace before the
	 * first tag.  so we must babysit it.
	 */
	while ((length > 0) && (isspace (*blob))) {
		blob++;
		length--;
	}

	blob[length] = '\0';

	doc = xmlParseMemory (blob, length);
	if (doc == NULL || doc->root == NULL ||
	    g_strcasecmp (doc->root->name, "location_data") != 0) {
		goto bad;
	}

	base = doc->root;

	wipe_redirect_table ();

	for (child = base->xmlChildrenNode; child; child = child->next) {
		if (g_strcasecmp (child->name, "location") == 0) {
			/* libxml sucks */
			name = NULL;
			uri = NULL;

			for (child2 = child->xmlChildrenNode; child2; child2 = child2->next) {
				if (g_strcasecmp (child2->name, "name") == 0) {
					name = xmlNodeGetContent (child2);
				}
				if (g_strcasecmp (child2->name, "uri") == 0) {
					uri = xmlNodeGetContent (child2);
				}
			}

			if ((name != NULL) && (uri != NULL)) {
				add_redirect (name, uri);
			} 
			g_free (name);
			g_free (uri);
		
		} 
	}
	xmlFreeDoc (doc);
	return TRUE;

bad:
	xmlFreeDoc (doc);
	return FALSE;
}


struct TrilobiteRedirectFetchHandle {
	TrilobiteReadFileHandle *handle;
	TrilobiteRedirectFetchCallback callback;
	gpointer callback_data;
};


static void
redirect_fetch_callback (GnomeVFSResult    result,
			 GnomeVFSFileSize  file_size,
			 char             *file_contents,
			 gpointer          callback_data)
{
	TrilobiteRedirectFetchHandle *handle;
	gboolean parsed_xml;

	parsed_xml = FALSE;

	handle = callback_data;

	if (result == GNOME_VFS_OK) {
		parsed_xml = trilobite_redirect_parse_xml (file_contents, file_size);
	}

	(*handle->callback) (result, parsed_xml, handle->callback_data);
	g_free (handle);
	g_free (file_contents);
}


TrilobiteRedirectFetchHandle *
trilobite_redirect_fetch_table_async (const char *uri,
				      TrilobiteRedirectFetchCallback callback,
				      gpointer callback_data)
{
	TrilobiteRedirectFetchHandle *handle;

	handle = g_new0 (TrilobiteRedirectFetchHandle, 1);

	handle->callback = callback;
	handle->callback_data = callback_data;

	handle->handle = trilobite_read_entire_file_async (uri, redirect_fetch_callback, handle);

	return handle;
}

void
trilobite_redirect_fetch_table_cancel (TrilobiteRedirectFetchHandle *handle)
{
	trilobite_read_file_cancel (handle->handle);
	g_free (handle);
}



/* find the uri for a redirect (you must free the string when done) */
char *
trilobite_redirect_lookup (const char *key)
{
	GError *error = NULL;
	char *full_key, *p;
	char *value;

	check_gconf_init ();
	full_key = g_strdup_printf ("%s/%s", REDIRECT_GCONF_PATH, key);

	/* convert all spaces to dashes */
	while ((p = strchr (full_key, ' ')) != NULL) {
		*p = '-';
	}

	value = gconf_engine_get_string (conf_engine, full_key, &error);
	if (error != NULL) {
		g_warning ("trilobite redirect: gconf can't find key '%s': %s", full_key, error->message);
		g_error_free (error);
	}

	g_free (full_key);
	return value;
}

/* find the default server hostname and port to use for eazel services
 * NOTE: this should be in "host:port" format, if the ':port' is missing,
 * the port should default to 443 (https)
 */
const char *
trilobite_get_services_address (void)
{
	GError *error = NULL;
	char *value;

	check_gconf_init ();
	value = gconf_engine_get_string (conf_engine, SERVICES_GCONF_PATH, &error);
	if ((value == NULL) || (error != NULL)) {
		if (error != NULL) {
			g_error_free (error);
		}
		value = SERVICES_DEFAULT_HOST;
	}

	return value;
}
