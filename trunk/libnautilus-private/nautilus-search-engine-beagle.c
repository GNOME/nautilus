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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include "nautilus-search-engine-beagle.h"
#include <beagle/beagle.h>

#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>

struct NautilusSearchEngineBeagleDetails {
	BeagleClient *client;
	NautilusQuery *query;

	BeagleQuery *current_query;
	char *current_query_uri_prefix;
	gboolean query_finished;
};


static void  nautilus_search_engine_beagle_class_init       (NautilusSearchEngineBeagleClass *class);
static void  nautilus_search_engine_beagle_init             (NautilusSearchEngineBeagle      *engine);

G_DEFINE_TYPE (NautilusSearchEngineBeagle,
	       nautilus_search_engine_beagle,
	       NAUTILUS_TYPE_SEARCH_ENGINE);

static NautilusSearchEngineClass *parent_class = NULL;

static void
finalize (GObject *object)
{
	NautilusSearchEngineBeagle *beagle;

	beagle = NAUTILUS_SEARCH_ENGINE_BEAGLE (object);
	
	if (beagle->details->current_query) {
		g_object_unref (beagle->details->current_query);
		beagle->details->current_query = NULL;
		g_free (beagle->details->current_query_uri_prefix);
		beagle->details->current_query_uri_prefix = NULL;
	}

	if (beagle->details->query) {
		g_object_unref (beagle->details->query);
		beagle->details->query = NULL;
	}

	if (beagle->details->client) {
		g_object_unref (beagle->details->client);
		beagle->details->client = NULL;
	}

	g_free (beagle->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
beagle_hits_added (BeagleQuery *query, 
		   BeagleHitsAddedResponse *response, 
		   NautilusSearchEngineBeagle *engine)
{
	GSList *hits, *list;
	GList *hit_uris;
	const char *uri;

	hit_uris = NULL;

	hits = beagle_hits_added_response_get_hits (response);
	
	for (list = hits; list != NULL; list = list->next) {
		BeagleHit *hit = BEAGLE_HIT (list->data);

		uri = beagle_hit_get_uri (hit);

		if (engine->details->current_query_uri_prefix &&
		    !g_str_has_prefix (uri, engine->details->current_query_uri_prefix)) {
			continue;
		}
		
		hit_uris = g_list_prepend (hit_uris, (char *)uri);
	}

	nautilus_search_engine_hits_added (NAUTILUS_SEARCH_ENGINE (engine), hit_uris);
	g_list_free (hit_uris);
}

static void
beagle_hits_subtracted (BeagleQuery *query, 
			BeagleHitsSubtractedResponse *response, 
			NautilusSearchEngineBeagle *engine)
{
	GSList *uris, *list;
	GList *hit_uris;

	hit_uris = NULL;

	uris = beagle_hits_subtracted_response_get_uris (response);
	
	for (list = uris; list != NULL; list = list->next) {
		hit_uris = g_list_prepend (hit_uris, (char *)list->data);
	}

	nautilus_search_engine_hits_subtracted (NAUTILUS_SEARCH_ENGINE (engine), hit_uris);
	g_list_free (hit_uris);
}

static void
beagle_finished (BeagleQuery *query, 
		 BeagleFinishedResponse *response,
		 NautilusSearchEngineBeagle *engine)
{
	/* For some reason we keep getting finished events,
	 * only emit finished once */
	if (engine->details->query_finished) {
		return;
	}
	
	engine->details->query_finished = TRUE;
	nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (engine));
}

static void
beagle_error (BeagleQuery *query,
	      GError *error,
	      NautilusSearchEngineBeagle *engine)
{
	nautilus_search_engine_error (NAUTILUS_SEARCH_ENGINE (engine), error->message);
}

static void
nautilus_search_engine_beagle_start (NautilusSearchEngine *engine)
{
	NautilusSearchEngineBeagle *beagle;
	GError *error;
	GList *mimetypes, *l;
	char *text, *mimetype;

	error = NULL;
	beagle = NAUTILUS_SEARCH_ENGINE_BEAGLE (engine);

	if (beagle->details->current_query) {
		return;
	}

	beagle->details->query_finished = FALSE;
	beagle->details->current_query = beagle_query_new ();
	g_signal_connect (beagle->details->current_query,
			  "hits-added", G_CALLBACK (beagle_hits_added), engine);
	g_signal_connect (beagle->details->current_query,
			  "hits-subtracted", G_CALLBACK (beagle_hits_subtracted), engine);
	g_signal_connect (beagle->details->current_query,
			  "finished", G_CALLBACK (beagle_finished), engine);
	g_signal_connect (beagle->details->current_query,
			  "error", G_CALLBACK (beagle_error), engine);

	/* We only want files */
	beagle_query_add_text (beagle->details->current_query," type:File");
				   
	beagle_query_set_max_hits (beagle->details->current_query,
				   1000);
	
	text = nautilus_query_get_text (beagle->details->query);
	beagle_query_add_text (beagle->details->current_query,
			       text);

	mimetypes = nautilus_query_get_mime_types (beagle->details->query);
	for (l = mimetypes; l != NULL; l = l->next) {
		char* temp;
		mimetype = l->data;
		temp = g_strconcat (" mimetype:", mimetype, NULL);
		beagle_query_add_text (beagle->details->current_query,temp);
		g_free (temp);
	}

	beagle->details->current_query_uri_prefix = nautilus_query_get_location (beagle->details->query);
	
	if (!beagle_client_send_request_async (beagle->details->client,
					       BEAGLE_REQUEST (beagle->details->current_query), &error)) {
		nautilus_search_engine_error (engine, error->message);
		g_error_free (error);
	}

	/* These must live during the lifetime of the query */
	g_free (text);
	eel_g_list_free_deep (mimetypes);
}

static void
nautilus_search_engine_beagle_stop (NautilusSearchEngine *engine)
{
	NautilusSearchEngineBeagle *beagle;

	beagle = NAUTILUS_SEARCH_ENGINE_BEAGLE (engine);

	if (beagle->details->current_query) {
		g_object_unref (beagle->details->current_query);
		beagle->details->current_query = NULL;
		g_free (beagle->details->current_query_uri_prefix);
		beagle->details->current_query_uri_prefix = NULL;
	}
}

static gboolean
nautilus_search_engine_beagle_is_indexed (NautilusSearchEngine *engine)
{
	return TRUE;
}

static void
nautilus_search_engine_beagle_set_query (NautilusSearchEngine *engine, NautilusQuery *query)
{
	NautilusSearchEngineBeagle *beagle;

	beagle = NAUTILUS_SEARCH_ENGINE_BEAGLE (engine);

	if (query) {
		g_object_ref (query);
	}

	if (beagle->details->query) {
		g_object_unref (beagle->details->query);
	}

	beagle->details->query = query;
}

static void
nautilus_search_engine_beagle_class_init (NautilusSearchEngineBeagleClass *class)
{
	GObjectClass *gobject_class;
	NautilusSearchEngineClass *engine_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NAUTILUS_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nautilus_search_engine_beagle_set_query;
	engine_class->start = nautilus_search_engine_beagle_start;
	engine_class->stop = nautilus_search_engine_beagle_stop;
	engine_class->is_indexed = nautilus_search_engine_beagle_is_indexed;
}

static void
nautilus_search_engine_beagle_init (NautilusSearchEngineBeagle *engine)
{
	engine->details = g_new0 (NautilusSearchEngineBeagleDetails, 1);
}


NautilusSearchEngine *
nautilus_search_engine_beagle_new (void)
{
	NautilusSearchEngineBeagle *engine;
	BeagleClient *client;
	
	if (!beagle_util_daemon_is_running ()) {
		/* check whether daemon is running as beagle_client_new
		 * doesn't fail when a stale socket file exists */
		return NULL;
	}

	client = beagle_client_new (NULL);

	if (client == NULL) {
		return NULL;
	}
	
	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_BEAGLE, NULL);
	
	engine->details->client = client;

	return NAUTILUS_SEARCH_ENGINE (engine);
}
