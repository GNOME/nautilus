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
#include <tracker.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>



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

	tracker_disconnect (tracker->details->client);

	g_free (tracker->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}


static void
search_callback (char **results, GError *error, gpointer user_data)
{
	NautilusSearchEngineTracker *tracker;
	char **results_p;
	GList *hit_uris;
	
	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (user_data);
	hit_uris = NULL;

	tracker->details->query_pending = FALSE;

	if (error) {
		nautilus_search_engine_error ( NAUTILUS_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		return;
	}

	if (!results) {
		return;
	}
	
	for (results_p = results; *results_p; results_p++) {
		
		char *uri;

		uri = g_filename_to_uri ((char *)*results_p, NULL, NULL);
		if (uri) {
			hit_uris = g_list_prepend (hit_uris, (char *)uri);
		}
	}

	nautilus_search_engine_hits_added (NAUTILUS_SEARCH_ENGINE (tracker), hit_uris);
	nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (tracker));
	g_strfreev  (results);
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
		location = g_filename_from_uri (location_uri, NULL, NULL);
		g_free (location_uri);
	} else {
		location = NULL;
	}

	mime_count  = g_list_length (mimetypes);

	i = 0;

	/* convert list into array */
	if (mime_count > 0) {

		mimes = g_new (char *, (mime_count + 1));
		
		for (l = mimetypes; l != NULL; l = l->next) {
			mimes[i] = g_strdup (l->data);
			i++;
		}

		mimes[mime_count] = NULL;			

		if (location) {
			tracker_search_metadata_by_text_and_mime_and_location_async (tracker->details->client,
										     search_text, (const char **)mimes, location,
										     search_callback, 
										     tracker);
			g_free (location);
		} else {
			tracker_search_metadata_by_text_and_mime_async (tracker->details->client, 
									search_text, (const char**)mimes, 
									search_callback,
									tracker);
		}

		g_strfreev (mimes);
		

	} else {
		if (location) {
			tracker_search_metadata_by_text_and_location_async (tracker->details->client,
									    search_text, 
									    location, 
								 	    search_callback,
									    tracker);
			g_free (location);
		} else {
			tracker_search_metadata_by_text_async (tracker->details->client, 
							       search_text, 
							       search_callback,
							       tracker);
		}
		
	}

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
	GError *err = NULL;

	tracker_client =  tracker_connect (FALSE);

	if (!tracker_client) {
		return NULL;
	}

	tracker_get_version (tracker_client, &err);

	if (err != NULL) {
		g_error_free (err);
		tracker_disconnect (tracker_client);
		return NULL;
	}

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_TRACKER, NULL);

	engine->details->client = tracker_client;

	engine->details->query_pending = FALSE;

	return NAUTILUS_SEARCH_ENGINE (engine);
}
