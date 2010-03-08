/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 */

#include <config.h>
#include "nautilus-search-engine-tracker.h"
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <gmodule.h>
#include <string.h>


typedef struct _TrackerClient TrackerClient;


/* tracker 0.6 API */
typedef void (*TrackerArrayReply) (char **result, GError *error, gpointer user_data);

static TrackerClient *	(*tracker_connect)		(gboolean enable_warnings) = NULL;
static TrackerClient *	(*tracker_connect_07)		(gboolean enable_warnings,
							 gint     timeout) = NULL;
static void		(*tracker_disconnect)		(TrackerClient *client) = NULL;
static void		(*tracker_cancel_last_call)	(TrackerClient *client) = NULL;
static int		(*tracker_get_version)		(TrackerClient *client, GError **error) = NULL;


static void (*tracker_search_metadata_by_text_async) (TrackerClient *client,
						      const char *query,
						      TrackerArrayReply callback,
						      gpointer user_data) = NULL;
static void (*tracker_search_metadata_by_text_and_mime_async) (TrackerClient *client,
							       const char *query,
							       const char **mimes,
							       TrackerArrayReply callback,
							       gpointer user_data) = NULL;
static void (*tracker_search_metadata_by_text_and_location_async) (TrackerClient *client,
								   const char *query,
								   const char *location,
								   TrackerArrayReply callback,
								   gpointer user_data) = NULL;
static void (*tracker_search_metadata_by_text_and_mime_and_location_async) (TrackerClient *client,
									    const char *query,
									    const char **mimes,
									    const char *location,
									    TrackerArrayReply callback,
									    gpointer user_data) = NULL;


/* tracker 0.8 API */
typedef enum {
	TRACKER_CLIENT_ENABLE_WARNINGS = 1 << 0
} TrackerClientFlags;

typedef void (*TrackerReplyGPtrArray) (GPtrArray *result,
				       GError    *error,
				       gpointer   user_data);

static TrackerClient *	(*tracker_client_new)			(TrackerClientFlags      flags,
								 gint                    timeout) = NULL;

static gboolean		(*tracker_cancel_call)			(TrackerClient          *client,
								 guint                   call_id) = NULL;

/*  -- we reuse tracker_cancel_last_call() from above, declaration is equal
 *
 *  static gboolean		(*tracker_cancel_last_call)		(TrackerClient          *client) = NULL;
 */

static gchar *		(*tracker_sparql_escape)		(const gchar            *str) = NULL;

static guint		(*tracker_resources_sparql_query_async)	(TrackerClient          *client,
								 const gchar            *query,
								 TrackerReplyGPtrArray   callback,
								 gpointer                user_data) = NULL;


#define MAP(a, b) { #a, (gpointer *)&a, b }

typedef struct {
	const char	*fn_name;
	gpointer	*fn_ptr_ref;
	gboolean	 mandatory;
} TrackerDlMapping;

static TrackerDlMapping tracker_dl_mapping[] = {
	MAP (tracker_connect, TRUE),
	MAP (tracker_disconnect, TRUE),
	MAP (tracker_cancel_last_call, TRUE),
	MAP (tracker_search_metadata_by_text_async, TRUE),
	MAP (tracker_search_metadata_by_text_and_mime_async, TRUE),
	MAP (tracker_search_metadata_by_text_and_location_async, TRUE),
	MAP (tracker_search_metadata_by_text_and_mime_and_location_async, TRUE),
	MAP (tracker_get_version, FALSE)
};

static TrackerDlMapping tracker_dl_mapping_08[] = {
	MAP (tracker_client_new, TRUE),
	MAP (tracker_cancel_call, TRUE),
	MAP (tracker_cancel_last_call, TRUE),
	MAP (tracker_sparql_escape, TRUE),
	MAP (tracker_resources_sparql_query_async, TRUE)
};
#undef MAP


static gboolean tracker_07;
static gboolean tracker_08;


static gboolean
load_symbols (GModule *tracker, TrackerDlMapping mapping[], gint num_elements)
{
	int i;

	for (i = 0; i < num_elements; i++) {
		if (! g_module_symbol (tracker, mapping[i].fn_name,
		                       mapping[i].fn_ptr_ref) &&
		      mapping[i].mandatory) {
			g_warning ("Missing symbol '%s' in libtracker\n",
				    mapping[i].fn_name);
			g_module_close (tracker);

			for (i = 0; i < num_elements; i++)
				mapping[i].fn_ptr_ref = NULL;

			return FALSE;
		}
	}

	return TRUE;
}

static void
open_libtracker (void)
{
	static gboolean done = FALSE;

	if (! done) {
		GModule *tracker;

		done = TRUE;
		tracker_07 = TRUE;
		tracker_08 = TRUE;

		tracker = g_module_open ("libtracker-client-0.7.so.0", G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
		if (! tracker) {
			tracker = g_module_open ("libtrackerclient.so.0", G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
			tracker_07 = FALSE;
			tracker_08 = FALSE;
		}
		if (! tracker)
			return;

		if (tracker_08)
			tracker_08 = load_symbols (tracker, tracker_dl_mapping_08, G_N_ELEMENTS (tracker_dl_mapping_08));
		if (! tracker_08) {
			if (! load_symbols (tracker, tracker_dl_mapping, G_N_ELEMENTS (tracker_dl_mapping)))
				return;
			if (tracker_07)
				tracker_connect_07 = (gpointer)tracker_connect;
		}
	}
}


struct NautilusSearchEngineTrackerDetails {
	NautilusQuery 	*query;
	TrackerClient 	*client;
	gboolean 	query_pending;
};


static void  nautilus_search_engine_tracker_class_init       (NautilusSearchEngineTrackerClass *class);
static void  nautilus_search_engine_tracker_init             (NautilusSearchEngineTracker      *engine);

G_DEFINE_TYPE (NautilusSearchEngineTracker,
	       nautilus_search_engine_tracker,
	       NAUTILUS_TYPE_SEARCH_ENGINE);

static NautilusSearchEngineClass *parent_class = NULL;

static void
finalize (GObject *object)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (object);
	
	if (tracker->details->query) {
		g_object_unref (tracker->details->query);
		tracker->details->query = NULL;
	}

	if (tracker_08) {
		g_object_unref (tracker->details->client);
	} else {
		tracker_disconnect (tracker->details->client);
	}

	g_free (tracker->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}


/* stolen from tracker sources, tracker.c */
static void
sparql_append_string_literal (GString     *sparql,
                              const gchar *str)
{
	char *s;

	s = tracker_sparql_escape (str);

	g_string_append_c (sparql, '"');
	g_string_append (sparql, s);
	g_string_append_c (sparql, '"');

	g_free (s);
}


static void
search_callback (gpointer results, GError *error, gpointer user_data)
{
	NautilusSearchEngineTracker *tracker;
	char **results_p;
	GPtrArray *OUT_result;
	GList *hit_uris;
	gint i;
	char *uri;
	
	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (user_data);
	hit_uris = NULL;

	tracker->details->query_pending = FALSE;

	if (error) {
		nautilus_search_engine_error (NAUTILUS_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		return;
	}

	if (! results) {
		return;
	}
	
	if (tracker_08) {
		/* new tracker 0.8 API */
		OUT_result = (GPtrArray*) results;

		for (i = 0; i < OUT_result->len; i++) {
			uri = g_strdup (((gchar **) OUT_result->pdata[i])[0]);
			if (uri) {
				hit_uris = g_list_prepend (hit_uris, (char *)uri);
			}
		}

		g_ptr_array_foreach (OUT_result, (GFunc) g_free, NULL);
		g_ptr_array_free (OUT_result, TRUE);

	} else {
		/* old tracker 0.6 API */
		for (results_p = results; *results_p; results_p++) {
			uri = tracker_07 ? g_strdup ((char *)*results_p) : g_filename_to_uri ((char *)*results_p, NULL, NULL);
			if (uri) {
				hit_uris = g_list_prepend (hit_uris, (char *)uri);
			}
		}
		g_strfreev ((gchar **)results);
	}

	nautilus_search_engine_hits_added (NAUTILUS_SEARCH_ENGINE (tracker), hit_uris);
	nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (tracker));
	eel_g_list_free_deep (hit_uris);
}


static void
nautilus_search_engine_tracker_start (NautilusSearchEngine *engine)
{
	NautilusSearchEngineTracker *tracker;
	GList 	*mimetypes, *l;
	char 	*search_text, *location, *location_uri;
	char 	**mimes;
	int 	i, mime_count;
	GString *sparql;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);


	if (tracker->details->query_pending) {
		return;
	}

	if (tracker->details->query == NULL) {
		return;
	}

	search_text = nautilus_query_get_text (tracker->details->query);

	mimetypes = nautilus_query_get_mime_types (tracker->details->query);

	location_uri = nautilus_query_get_location (tracker->details->query);

	if (location_uri) {
		location = (tracker_07 && !tracker_08) ? g_strdup (location_uri) : g_filename_from_uri (location_uri, NULL, NULL);
		g_free (location_uri);
	} else {
		location = NULL;
	}

	mime_count  = g_list_length (mimetypes);

	i = 0;
	sparql = NULL;

	if (tracker_08) {
		/* new tracker 0.8 API */
		if (mime_count > 0) {
			sparql = g_string_new ("SELECT nie:url(?file) WHERE { ?file a nfo:FileDataObject ; nie:mimeType ?mime ; fts:match ");
			sparql_append_string_literal (sparql, search_text);

			g_string_append (sparql, " . FILTER (");
			if (location) {
				g_string_append (sparql, "fn:starts-with(?file,");
				sparql_append_string_literal (sparql, location);
				g_string_append (sparql, ")");
				g_string_append (sparql, " && (");
			}


			for (l = mimetypes; l != NULL; l = l->next) {
				if (l != mimetypes) {
					g_string_append (sparql, " || ");
				}

				g_string_append (sparql, "?mime = ");
				sparql_append_string_literal (sparql, l->data);
			}

			if (location)
				g_string_append (sparql, ")");
			g_string_append (sparql, ") }");
		} else {
			sparql = g_string_new ("SELECT nie:url(?file) WHERE { ?file a nfo:FileDataObject ; fts:match ");
			sparql_append_string_literal (sparql, search_text);
			if (location) {
				g_string_append (sparql, " . FILTER (fn:starts-with(?file,");
				sparql_append_string_literal (sparql, location);
				g_string_append (sparql, "))");
			}
			g_string_append (sparql, " }");
		}

		tracker_resources_sparql_query_async (tracker->details->client, sparql->str, (TrackerReplyGPtrArray) search_callback, tracker);
		g_string_free (sparql, TRUE);
	} else {
		/* old tracker 0.6 API */
		if (mime_count > 0) {
			/* convert list into array */
			mimes = g_new (char *, (mime_count + 1));

			for (l = mimetypes; l != NULL; l = l->next) {
				mimes[i] = g_strdup (l->data);
				i++;
			}

			mimes[mime_count] = NULL;

			if (location) {
				tracker_search_metadata_by_text_and_mime_and_location_async (tracker->details->client,
											     search_text, (const char **)mimes, location,
											     (TrackerArrayReply) search_callback,
											     tracker);
			} else {
				tracker_search_metadata_by_text_and_mime_async (tracker->details->client,
										search_text, (const char**)mimes,
										(TrackerArrayReply) search_callback,
										tracker);
			}

			g_strfreev (mimes);


		} else {
			if (location) {
				tracker_search_metadata_by_text_and_location_async (tracker->details->client,
										    search_text,
										    location,
										    (TrackerArrayReply) search_callback,
										    tracker);
			} else {
				tracker_search_metadata_by_text_async (tracker->details->client,
								       search_text,
								       (TrackerArrayReply) search_callback,
								       tracker);
			}

		}
	}

	g_free (location);

	tracker->details->query_pending = TRUE;
	g_free (search_text);
	eel_g_list_free_deep (mimetypes);
}

static void
nautilus_search_engine_tracker_stop (NautilusSearchEngine *engine)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);
	
	if (tracker->details->query && tracker->details->query_pending) {
		tracker_cancel_last_call (tracker->details->client);
		tracker->details->query_pending = FALSE;
	}
}

static gboolean
nautilus_search_engine_tracker_is_indexed (NautilusSearchEngine *engine)
{
	return TRUE;
}

static void
nautilus_search_engine_tracker_set_query (NautilusSearchEngine *engine, NautilusQuery *query)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);

	if (query) {
		g_object_ref (query);
	}

	if (tracker->details->query) {
		g_object_unref (tracker->details->query);
	}

	tracker->details->query = query;
}

static void
nautilus_search_engine_tracker_class_init (NautilusSearchEngineTrackerClass *class)
{
	GObjectClass *gobject_class;
	NautilusSearchEngineClass *engine_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NAUTILUS_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nautilus_search_engine_tracker_set_query;
	engine_class->start = nautilus_search_engine_tracker_start;
	engine_class->stop = nautilus_search_engine_tracker_stop;
	engine_class->is_indexed = nautilus_search_engine_tracker_is_indexed;
}

static void
nautilus_search_engine_tracker_init (NautilusSearchEngineTracker *engine)
{
	engine->details = g_new0 (NautilusSearchEngineTrackerDetails, 1);
}


NautilusSearchEngine *
nautilus_search_engine_tracker_new (void)
{
	NautilusSearchEngineTracker *engine;
	TrackerClient *tracker_client;

	open_libtracker ();

	if (tracker_08) {
		tracker_client = tracker_client_new (TRACKER_CLIENT_ENABLE_WARNINGS, G_MAXINT);
	} else {
		if (! tracker_connect)
			return NULL;

		if (tracker_07) {
			tracker_client = tracker_connect_07 (FALSE, -1);
		} else {
			tracker_client = tracker_connect (FALSE);
		}
	}

	if (!tracker_client) {
		return NULL;
	}

	if (! tracker_07 && ! tracker_08) {
		GError *err = NULL;

		tracker_get_version (tracker_client, &err);

		if (err != NULL) {
			g_error_free (err);
			tracker_disconnect (tracker_client);
			return NULL;
		}
	}

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_TRACKER, NULL);

	engine->details->client = tracker_client;

	engine->details->query_pending = FALSE;

	return NAUTILUS_SEARCH_ENGINE (engine);
}
