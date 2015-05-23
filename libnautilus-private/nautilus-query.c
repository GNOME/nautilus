/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <glib/gi18n.h>

#include "nautilus-file-utilities.h"
#include "nautilus-query.h"

struct NautilusQueryDetails {
	char *text;
	char *location_uri;
	GList *mime_types;
	gboolean show_hidden;

	char **prepared_words;
};

static void  nautilus_query_class_init       (NautilusQueryClass *class);
static void  nautilus_query_init             (NautilusQuery      *query);

G_DEFINE_TYPE (NautilusQuery, nautilus_query, G_TYPE_OBJECT);

static void
finalize (GObject *object)
{
	NautilusQuery *query;

	query = NAUTILUS_QUERY (object);
	g_free (query->details->text);
	g_strfreev (query->details->prepared_words);
	g_free (query->details->location_uri);

	G_OBJECT_CLASS (nautilus_query_parent_class)->finalize (object);
}

static void
nautilus_query_class_init (NautilusQueryClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	g_type_class_add_private (class, sizeof (NautilusQueryDetails));
}

static void
nautilus_query_init (NautilusQuery *query)
{
	query->details = G_TYPE_INSTANCE_GET_PRIVATE (query, NAUTILUS_TYPE_QUERY,
						      NautilusQueryDetails);
	query->details->show_hidden = TRUE;
	query->details->location_uri = nautilus_get_home_directory_uri ();
}

static gchar *
prepare_string_for_compare (const gchar *string)
{
	gchar *normalized, *res;

	normalized = g_utf8_normalize (string, -1, G_NORMALIZE_NFD);
	res = g_utf8_strdown (normalized, -1);
	g_free (normalized);

	return res;
}

gdouble
nautilus_query_matches_string (NautilusQuery *query,
			       const gchar *string)
{
	gchar *prepared_string, *ptr;
	gboolean found;
	gdouble retval;
	gint idx, nonexact_malus;

	if (!query->details->text) {
		return -1;
	}

	if (!query->details->prepared_words) {
		prepared_string = prepare_string_for_compare (query->details->text);
		query->details->prepared_words = g_strsplit (prepared_string, " ", -1);
		g_free (prepared_string);
	}

	prepared_string = prepare_string_for_compare (string);
	found = TRUE;
	ptr = NULL;
	nonexact_malus = 0;

	for (idx = 0; query->details->prepared_words[idx] != NULL; idx++) {
		if ((ptr = strstr (prepared_string, query->details->prepared_words[idx])) == NULL) {
			found = FALSE;
			break;
		}

		nonexact_malus += strlen (ptr) - strlen (query->details->prepared_words[idx]);
	}

	if (!found) {
		g_free (prepared_string);
		return -1;
	}

	retval = MAX (10.0, 50.0 - (gdouble) (ptr - prepared_string) - nonexact_malus);
	g_free (prepared_string);

	return retval;
}

NautilusQuery *
nautilus_query_new (void)
{
	return g_object_new (NAUTILUS_TYPE_QUERY,  NULL);
}


char *
nautilus_query_get_text (NautilusQuery *query)
{
	return g_strdup (query->details->text);
}

void 
nautilus_query_set_text (NautilusQuery *query, const char *text)
{
	g_free (query->details->text);
	query->details->text = g_strstrip (g_strdup (text));

	g_strfreev (query->details->prepared_words);
	query->details->prepared_words = NULL;
}

char *
nautilus_query_get_location (NautilusQuery *query)
{
	return g_strdup (query->details->location_uri);
}
	
void
nautilus_query_set_location (NautilusQuery *query, const char *uri)
{
	g_free (query->details->location_uri);
	query->details->location_uri = g_strdup (uri);
}

GList *
nautilus_query_get_mime_types (NautilusQuery *query)
{
	return g_list_copy_deep (query->details->mime_types, (GCopyFunc) g_strdup, NULL);
}

void
nautilus_query_set_mime_types (NautilusQuery *query, GList *mime_types)
{
	g_list_free_full (query->details->mime_types, g_free);
	query->details->mime_types = g_list_copy_deep (mime_types, (GCopyFunc) g_strdup, NULL);
}

void
nautilus_query_add_mime_type (NautilusQuery *query, const char *mime_type)
{
	query->details->mime_types = g_list_append (query->details->mime_types,
						    g_strdup (mime_type));
}

gboolean
nautilus_query_get_show_hidden_files (NautilusQuery *query)
{
	return query->details->show_hidden;
}

void
nautilus_query_set_show_hidden_files (NautilusQuery *query, gboolean show_hidden)
{
	query->details->show_hidden = show_hidden;
}

char *
nautilus_query_to_readable_string (NautilusQuery *query)
{
	if (!query || !query->details->text || query->details->text[0] == '\0') {
		return g_strdup (_("Search"));
	}

	return g_strdup_printf (_("Search for “%s”"), query->details->text);
}
