/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-search-engine-simple.h"

#include <string.h>
#include <glib/gstrfuncs.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <libgnomevfs/gnome-vfs-directory.h>

#define BATCH_SIZE 500

typedef struct {
	NautilusSearchEngineSimple *engine;

	GnomeVFSURI *uri;
	GList *mime_types;
	char **words;
	GList *found_list;
	
	gint n_processed_files;
	GList *uri_hits;

	/* accessed on both threads: */
	volatile gboolean cancelled;
} SearchThreadData;


struct NautilusSearchEngineSimpleDetails {
	NautilusQuery *query;

	SearchThreadData *active_search;
	
	gboolean query_finished;
};


static void  nautilus_search_engine_simple_class_init       (NautilusSearchEngineSimpleClass *class);
static void  nautilus_search_engine_simple_init             (NautilusSearchEngineSimple      *engine);

G_DEFINE_TYPE (NautilusSearchEngineSimple,
	       nautilus_search_engine_simple,
	       NAUTILUS_TYPE_SEARCH_ENGINE);

static NautilusSearchEngineClass *parent_class = NULL;

static void
finalize (GObject *object)
{
	NautilusSearchEngineSimple *simple;

	simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (object);
	
	if (simple->details->query) {
		g_object_unref (simple->details->query);
		simple->details->query = NULL;
	}

	g_free (simple->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static SearchThreadData *
search_thread_data_new (NautilusSearchEngineSimple *engine,
			NautilusQuery *query)
{
	SearchThreadData *data;
	char *text, *lower, *normalized, *uri;

	data = g_new0 (SearchThreadData, 1);

	data->engine = engine;
	uri = nautilus_query_get_location (query);
	if (uri != NULL) {
		data->uri = gnome_vfs_uri_new (uri);
		g_free (uri);
	}
	if (data->uri == NULL) {
		data->uri = gnome_vfs_uri_new ("file:///");
	}
	
	text = nautilus_query_get_text (query);
	normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);
	lower = g_utf8_strdown (normalized, -1);
	data->words = g_strsplit (lower, " ", -1);
	g_free (text);
	g_free (lower);
	g_free (normalized);

	data->mime_types = nautilus_query_get_mime_types (query);

	return data;
}

static void 
search_thread_data_free (SearchThreadData *data)
{
	gnome_vfs_uri_unref (data->uri);
	g_strfreev (data->words);	
	eel_g_list_free_deep (data->mime_types);
	g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
	SearchThreadData *data;

	data = user_data;

	if (!data->cancelled) {
		nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (data->engine));
		data->engine->details->active_search = NULL;
	}
	
	search_thread_data_free (data);
	
	return FALSE;
}

typedef struct {
	GList *uris;
	SearchThreadData *thread_data;
} SearchHits;


static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
	SearchHits *hits;

	hits = user_data;

	if (!hits->thread_data->cancelled) {
		nautilus_search_engine_hits_added (NAUTILUS_SEARCH_ENGINE (hits->thread_data->engine),
						   hits->uris);
	}
	
	eel_g_list_free_deep (hits->uris);
	g_free (hits);
	
	return FALSE;
}

static void
send_batch (SearchThreadData *data)
{
	SearchHits *hits;
	
	data->n_processed_files = 0;
	
	if (data->uri_hits) {
		hits = g_new (SearchHits, 1);
		hits->uris = data->uri_hits;
		hits->thread_data = data;
		g_idle_add (search_thread_add_hits_idle, hits);
	}
	data->uri_hits = NULL;
}

static gboolean
search_visit_func (const gchar *rel_path,
		   GnomeVFSFileInfo *info,
		   gboolean recursing_will_loop,
		   gpointer user_data,
		   gboolean *recurse)
{
	SearchThreadData *data;
	int i;
	char *lower_name, *normalized;
	GnomeVFSURI *uri;
	gboolean hit;
	GList *l;
	gboolean is_hidden;
	
	data = user_data;

	if (data->cancelled) {
		return FALSE;
	}

	is_hidden = *info->name == '.';
	
	if (recursing_will_loop || is_hidden) {
		*recurse = FALSE;
	} else {
		*recurse = TRUE;
	}

	hit = FALSE;

	if (!is_hidden) {
		if (g_utf8_validate (info->name, -1, NULL)) {
			normalized = g_utf8_normalize (info->name, -1, G_NORMALIZE_NFD);
			lower_name = g_utf8_strdown (normalized, -1);
			g_free (normalized);
		} else {
			lower_name = g_ascii_strdown (info->name, -1);
		}
		
		hit = TRUE;
		for (i = 0; data->words[i] != NULL; i++) {
			if (strstr (lower_name, data->words[i]) == NULL) {
				hit = FALSE;
				break;
			}
		}
		g_free (lower_name);
	}

	if (hit && data->mime_types != NULL) {
		hit = FALSE;

		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) {
			for (l = data->mime_types; l != NULL; l = l->next) {
				if (strcmp (info->mime_type, l->data) == 0) {
					hit = TRUE;
					break;
				}
			}
		}
	}

	if (hit) {
		uri  = gnome_vfs_uri_append_string (data->uri, rel_path);
		data->uri_hits = g_list_prepend (data->uri_hits, gnome_vfs_uri_to_string (uri, 0));
		gnome_vfs_uri_unref (uri);
	}

	data->n_processed_files++;

	if (data->n_processed_files > BATCH_SIZE) {
		send_batch (data);
	}
	
		 
	return TRUE;
}



static gpointer 
search_thread_func (gpointer user_data)
{
	SearchThreadData *data;
	GnomeVFSResult res;

	data = user_data;

	res = gnome_vfs_directory_visit_uri (data->uri,
					     GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
					     GNOME_VFS_DIRECTORY_VISIT_LOOPCHECK,
					     search_visit_func,
					     data);
	send_batch (data);

	g_idle_add (search_thread_done_idle, data);
	
	return NULL;
}

static void
nautilus_search_engine_simple_start (NautilusSearchEngine *engine)
{
	NautilusSearchEngineSimple *simple;
	SearchThreadData *data;
	
	simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (engine);

	if (simple->details->active_search != NULL) {
		return;
	}

	if (simple->details->query == NULL) {
		return;
	}
	
	data = search_thread_data_new (simple, simple->details->query);

	g_thread_create (search_thread_func, data, FALSE, NULL);

	simple->details->active_search = data;
}

static void
nautilus_search_engine_simple_stop (NautilusSearchEngine *engine)
{
	NautilusSearchEngineSimple *simple;

	simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (engine);

	if (simple->details->active_search != NULL) {
		simple->details->active_search->cancelled = TRUE;
		simple->details->active_search = NULL;
	}
}

static gboolean
nautilus_search_engine_simple_is_indexed (NautilusSearchEngine *engine)
{
	return FALSE;
}

static void
nautilus_search_engine_simple_set_query (NautilusSearchEngine *engine, NautilusQuery *query)
{
	NautilusSearchEngineSimple *simple;

	simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (engine);

	if (query) {
		g_object_ref (query);
	}

	if (simple->details->query) {
		g_object_unref (simple->details->query);
	}

	simple->details->query = query;
}

static void
nautilus_search_engine_simple_class_init (NautilusSearchEngineSimpleClass *class)
{
	GObjectClass *gobject_class;
	NautilusSearchEngineClass *engine_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NAUTILUS_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nautilus_search_engine_simple_set_query;
	engine_class->start = nautilus_search_engine_simple_start;
	engine_class->stop = nautilus_search_engine_simple_stop;
	engine_class->is_indexed = nautilus_search_engine_simple_is_indexed;
}

static void
nautilus_search_engine_simple_init (NautilusSearchEngineSimple *engine)
{
	engine->details = g_new0 (NautilusSearchEngineSimpleDetails, 1);
}


NautilusSearchEngine *
nautilus_search_engine_simple_new (void)
{
	NautilusSearchEngine *engine;

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

	return engine;
}
