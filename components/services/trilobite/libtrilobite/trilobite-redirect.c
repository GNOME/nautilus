/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 *  trilobite-redirect: functions to fetch a redirection table from
 *  a remote xml file, store it in gconf, and then lookup entries
 *  later.  this may only be useful for eazel services.
 *
 *  Copyright (C) 2000 Eazel, Inc
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gnome-xml/parser.h>
#include <gconf/gconf.h>
#include <gconf/gconf-engine.h>
#include "trilobite-core-utils.h"
#include "trilobite-redirect.h"

#if 0
#define REDIRECT_TABLE_URL	"http://drig.eazel.com:8888/my/redirect"
#else
#define REDIRECT_TABLE_URL	"eazel-auth:/table.xml"
#endif
#define REDIRECT_GCONF_PATH	"/apps/eazel-trilobite/redirect-table"

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
	GConfError *error = NULL;

	if (! gconf_is_initialized ()) {
		char *argv[] = { "trilobite", NULL };

		if (! gconf_init (1, argv, &error)) {
			g_assert (error != NULL);
			g_warning ("gconf init error: %s", error->str);
			gconf_error_destroy (error);
		}

		conf_engine = gconf_engine_get_default ();
		g_atexit (trilobite_redirect_done);
	}
}


/* erase everything currently in our gconf table (so we can add new entries now) */
static void
wipe_redirect_table (void)
{
	GSList *list, *iter;
	GConfError *error = NULL;
	GConfEntry *entry;
	char *doomed_key;

	check_gconf_init ();
	list = gconf_all_entries (conf_engine, REDIRECT_GCONF_PATH, &error);
	if (error != NULL) {
		g_warning ("gconf wipe error: %s", error->str);
		gconf_error_destroy (error);
		return;
	}

	for (iter = list; iter; iter = g_slist_next (iter)) {
		entry = (GConfEntry *) (iter->data);
		doomed_key = g_strdup_printf ("%s/%s", REDIRECT_GCONF_PATH, gconf_entry_key (entry));
		gconf_unset (conf_engine, doomed_key, &error);
		if (error != NULL) {
			g_warning ("trilobite redirect: gconf couldn't delete key '%s': %s", doomed_key, error->str);
			gconf_error_destroy (error);
		}
		g_free (doomed_key);
		gconf_entry_destroy (entry);
	}
	g_slist_free (list);
}

static void
add_redirect (const char *key, const char *value)
{
	GConfError *error = NULL;
	char *full_key, *p;

	check_gconf_init ();
	full_key = g_strdup_printf ("%s/%s", REDIRECT_GCONF_PATH, key);

	/* convert all spaces to dashes */
	while ((p = strchr (full_key, ' ')) != NULL) {
		*p = '-';
	}

	gconf_set_string (conf_engine, full_key, value, &error);
	if (error != NULL) {
		g_warning ("trilobite redirect: gconf can't add key '%s': %s", full_key, error->str);
		gconf_error_destroy (error);
	}
	g_free (full_key);
}

/* parse an xml file into the redirect table */
static void
trilobite_redirect_parse_xml (char *blob, int length)
{
	xmlDocPtr doc;
	xmlNodePtr base, child;
	char *name, *uri;

	g_return_if_fail (blob != NULL);
	g_return_if_fail (length > 0);

	doc = xmlParseMemory (blob, length);
	if (doc == NULL) {
		g_warning ("trilobite redirect: bad XML: '%s'", blob);
		return;
	}

	base = doc->root;
	if (base == NULL) {
		g_warning ("trilobite redirect: empty XML!");
		goto out;
	}
	if (g_strcasecmp (base->name, "redirect-table") != 0) {
		g_warning ("trilobite redirect: no redirect table!");
		goto bad;
	}

	for (child = base->childs; child; child = child->next) {
		if (g_strcasecmp (child->name, "redirect") == 0) {
			name = xmlGetProp (child, "name");
			uri = xmlGetProp (child, "uri");

			trilobite_debug ("trilobite redirect: %s -> %s", name, uri);
			add_redirect (name, uri);
		} else {
			g_warning ("trilobite redirect: ignoring directive '%s'", child->name);
		}
	}
	xmlFreeDoc (doc);
	return;

bad:
	g_warning ("trilobite redirect: dysfunctional XML was '%s'", blob);

out:
	xmlFreeDoc (doc);
}



/* fetch xml file all at once, blocking until we have some closure.
 * you will definitely want to ref any objects you own before calling this function,
 * since it involves iterations of gtk_main.
 */
gboolean
trilobite_redirect_fetch_table (void)
{
	char *body;
	int length;

	if (! trilobite_fetch_uri (REDIRECT_TABLE_URL, &body, &length)) {
		return FALSE;
	}

	wipe_redirect_table ();
	trilobite_redirect_parse_xml (body, length);
	return TRUE;
}


/* find the url for a redirect (you must free the string when done) */
char *
trilobite_redirect_lookup (const char *key)
{
	GConfError *error = NULL;
	char *full_key, *p;
	char *value;

	check_gconf_init ();
	full_key = g_strdup_printf ("%s/%s", REDIRECT_GCONF_PATH, key);

	/* convert all spaces to dashes */
	while ((p = strchr (full_key, ' ')) != NULL) {
		*p = '-';
	}

	value = gconf_get_string (conf_engine, full_key, &error);
	if (error != NULL) {
		g_warning ("trilobite redirect: gconf can't find key '%s': %s", full_key, error->str);
		gconf_error_destroy (error);
	}

	g_free (full_key);
	return value;
}
