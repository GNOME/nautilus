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
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */
#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "nautilus-search-engine-simple.h"

#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define BATCH_SIZE 500
#define CREATE_THREAD_DELAY_MS 500

enum
{
    PROP_0,
    PROP_RUNNING,
    NUM_PROPERTIES
};

typedef struct
{
    NautilusSearchEngineSimple *engine;
    GCancellable *cancellable;

    GPtrArray *mime_types;
    GList *found_list;

    GQueue *directories;     /* GFiles */

    GHashTable *visited;

    gint n_processed_files;
    GList *hits;

    NautilusQuery *query;
    gint processing_id;
    GMutex idle_mutex;
    /* The following data can be accessed from different threads
     * and needs to lock the mutex
     */
    GQueue *idle_queue;
    gboolean finished;
} SearchThreadData;


struct _NautilusSearchEngineSimple
{
    GObject parent_instance;
    NautilusQuery *query;
    guint create_thread_timeout_id;

    SearchThreadData *active_search;
};

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineSimple,
                         nautilus_search_engine_simple,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

static void
finalize (GObject *object)
{
    NautilusSearchEngineSimple *simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (object);
    g_clear_object (&simple->query);
    g_clear_handle_id (&simple->create_thread_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nautilus_search_engine_simple_parent_class)->finalize (object);
}

static SearchThreadData *
search_thread_data_new (NautilusSearchEngineSimple *engine,
                        NautilusQuery              *query)
{
    SearchThreadData *data;

    data = g_new0 (SearchThreadData, 1);

    data->engine = g_object_ref (engine);
    data->directories = g_queue_new ();
    data->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    data->query = g_object_ref (query);
    data->mime_types = nautilus_query_get_mime_types (query);

    data->cancellable = g_cancellable_new ();

    g_mutex_init (&data->idle_mutex);
    data->idle_queue = g_queue_new ();

    return data;
}

static void
search_thread_data_free (SearchThreadData *data)
{
    GList *hits;

    g_queue_foreach (data->directories,
                     (GFunc) g_object_unref, NULL);
    g_queue_free (data->directories);
    g_hash_table_destroy (data->visited);
    g_object_unref (data->cancellable);
    g_object_unref (data->query);
    g_clear_pointer (&data->mime_types, g_ptr_array_unref);
    g_list_free_full (data->hits, g_object_unref);
    g_object_unref (data->engine);
    g_mutex_clear (&data->idle_mutex);

    while ((hits = g_queue_pop_head (data->idle_queue)))
    {
        g_list_free_full (hits, g_object_unref);
    }
    g_queue_free (data->idle_queue);

    g_free (data);
}

static gboolean
search_thread_done (SearchThreadData *data)
{
    NautilusSearchEngineSimple *engine = data->engine;

    if (g_cancellable_is_cancelled (data->cancellable))
    {
        g_debug ("Simple engine finished and cancelled");
    }
    else
    {
        g_debug ("Simple engine finished");
    }
    engine->active_search = NULL;
    nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (engine),
                                       NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);

    g_object_notify (G_OBJECT (engine), "running");

    search_thread_data_free (data);

    return G_SOURCE_REMOVE;
}

static void
search_thread_process_hits_idle (SearchThreadData *data,
                                 GList            *hits)
{
    if (!g_cancellable_is_cancelled (data->cancellable))
    {
        g_debug ("Simple engine add hits");
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (data->engine),
                                             hits);
    }
}

static gboolean
search_thread_process_idle (gpointer user_data)
{
    SearchThreadData *thread_data;
    GList *hits;

    thread_data = user_data;

    g_mutex_lock (&thread_data->idle_mutex);
    hits = g_queue_pop_head (thread_data->idle_queue);
    /* Even if the cancellable is cancelled, we need to make sure the search
     * thread has aknowledge it, and therefore not using the thread data after
     * freeing it. The search thread will mark as finished whenever the search
     * is finished or cancelled.
     * Nonetheless, we should stop yielding results if the search was cancelled
     */
    if (thread_data->finished)
    {
        if (hits == NULL || g_cancellable_is_cancelled (thread_data->cancellable))
        {
            g_mutex_unlock (&thread_data->idle_mutex);

            if (hits)
            {
                g_list_free_full (hits, g_object_unref);
            }
            search_thread_done (thread_data);

            return G_SOURCE_REMOVE;
        }
    }

    g_mutex_unlock (&thread_data->idle_mutex);

    if (hits)
    {
        search_thread_process_hits_idle (thread_data, hits);
        g_list_free_full (hits, g_object_unref);
    }

    return G_SOURCE_CONTINUE;
}

static void
finish_search_thread (SearchThreadData *thread_data)
{
    g_mutex_lock (&thread_data->idle_mutex);
    thread_data->finished = TRUE;
    g_mutex_unlock (&thread_data->idle_mutex);

    /* If no results were processed, direclty finish the search, in the main
     * thread.
     */
    if (thread_data->processing_id == 0)
    {
        g_idle_add (G_SOURCE_FUNC (search_thread_done), thread_data);
    }
}

static void
process_batch_in_idle (SearchThreadData *thread_data,
                       GList            *hits)
{
    g_return_if_fail (hits != NULL);

    g_mutex_lock (&thread_data->idle_mutex);
    g_queue_push_tail (thread_data->idle_queue, hits);
    g_mutex_unlock (&thread_data->idle_mutex);

    if (thread_data->processing_id == 0)
    {
        thread_data->processing_id = g_idle_add (search_thread_process_idle, thread_data);
    }
}

static void
send_batch_in_idle (SearchThreadData *thread_data)
{
    thread_data->n_processed_files = 0;

    if (thread_data->hits)
    {
        process_batch_in_idle (thread_data, thread_data->hits);
    }
    thread_data->hits = NULL;
}

#define STD_ATTRIBUTES \
        G_FILE_ATTRIBUTE_STANDARD_NAME "," \
        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
        G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
        G_FILE_ATTRIBUTE_TIME_ACCESS "," \
        G_FILE_ATTRIBUTE_TIME_CREATED "," \
        G_FILE_ATTRIBUTE_ID_FILE

static void
visit_directory (GFile            *dir,
                 SearchThreadData *data)
{
    g_autoptr (GPtrArray) date_range = NULL;
    NautilusQuerySearchType type;
    NautilusQueryRecursive recursive_flag;
    GFileEnumerator *enumerator;
    GFileInfo *info;
    GFile *child;
    const char *mime_type, *display_name;
    gdouble match;
    gboolean is_hidden, found;
    const char *id;
    gboolean visited;
    GDateTime *initial_date;
    GDateTime *end_date;
    gchar *uri;

    enumerator = g_file_enumerate_children (dir,
                                            data->mime_types->len > 0 ?
                                            STD_ATTRIBUTES ","
                                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                            G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE
                                            :
                                            STD_ATTRIBUTES
                                            ,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            data->cancellable, NULL);

    if (enumerator == NULL)
    {
        return;
    }

    type = nautilus_query_get_search_type (data->query);
    recursive_flag = nautilus_query_get_recursive (data->query);
    date_range = nautilus_query_get_date_range (data->query);

    while ((info = g_file_enumerator_next_file (enumerator, data->cancellable, NULL)) != NULL)
    {
        g_autoptr (GDateTime) mtime = NULL;
        g_autoptr (GDateTime) atime = NULL;
        g_autoptr (GDateTime) ctime = NULL;
        gboolean recursive = FALSE;

        display_name = g_file_info_get_display_name (info);
        if (display_name == NULL)
        {
            goto next;
        }

        if (!nautilus_query_get_show_hidden_files (data->query))
        {
            is_hidden = g_file_info_get_attribute_boolean (info,
                                                           G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                        g_file_info_get_attribute_boolean (info,
                                                           G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);

            if (is_hidden)
            {
                goto next;
            }
        }

        child = g_file_get_child (dir, g_file_info_get_name (info));
        match = nautilus_query_matches_string (data->query, display_name);
        found = (match > -1);

        if (found && data->mime_types->len > 0)
        {
            mime_type = g_file_info_get_attribute_string (info,
                                                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
            if (mime_type == NULL)
            {
                mime_type = g_file_info_get_attribute_string (info,
                                                              G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
            }

            found = FALSE;

            for (guint i = 0; mime_type != NULL && i < data->mime_types->len; i++)
            {
                if (g_content_type_is_a (mime_type, g_ptr_array_index (data->mime_types, i)))
                {
                    found = TRUE;
                    break;
                }
            }
        }

        mtime = g_file_info_get_modification_date_time (info);
        atime = g_file_info_get_access_date_time (info);
        ctime = g_file_info_get_creation_date_time (info);

        if (found && date_range != NULL)
        {
            GDateTime *target_date;

            initial_date = g_ptr_array_index (date_range, 0);
            end_date = g_ptr_array_index (date_range, 1);

            switch (type)
            {
                case NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS:
                {
                    target_date = atime;
                }
                break;

                case NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED:
                {
                    target_date = mtime;
                }
                break;

                case NAUTILUS_QUERY_SEARCH_TYPE_CREATED:
                {
                    target_date = ctime;
                }
                break;

                default:
                {
                    target_date = NULL;
                }
            }

            found = nautilus_date_time_is_between_dates (target_date,
                                                         initial_date,
                                                         end_date);
        }

        if (found)
        {
            NautilusSearchHit *hit;

            uri = g_file_get_uri (child);
            hit = nautilus_search_hit_new (uri);
            g_free (uri);
            nautilus_search_hit_set_fts_rank (hit, match);
            nautilus_search_hit_set_modification_time (hit, mtime);
            nautilus_search_hit_set_access_time (hit, atime);
            nautilus_search_hit_set_creation_time (hit, ctime);

            data->hits = g_list_prepend (data->hits, hit);
        }

        data->n_processed_files++;
        if (data->n_processed_files > BATCH_SIZE)
        {
            send_batch_in_idle (data);
        }

        if (recursive_flag != NAUTILUS_QUERY_RECURSIVE_NEVER &&
            g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
            if (recursive_flag == NAUTILUS_QUERY_RECURSIVE_ALWAYS)
            {
                recursive = TRUE;
            }
            else if (recursive_flag == NAUTILUS_QUERY_RECURSIVE_LOCAL_ONLY)
            {
                g_autoptr (GFileInfo) file_system_info = NULL;

                file_system_info = g_file_query_filesystem_info (child,
                                                                 G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE,
                                                                 NULL, NULL);
                if (file_system_info != NULL)
                {
                    recursive = !g_file_info_get_attribute_boolean (file_system_info,
                                                                    G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE);
                }
            }
        }

        if (recursive)
        {
            id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
            visited = FALSE;
            if (id)
            {
                if (g_hash_table_lookup_extended (data->visited,
                                                  id, NULL, NULL))
                {
                    visited = TRUE;
                }
                else
                {
                    g_hash_table_insert (data->visited, g_strdup (id), NULL);
                }
            }

            if (!visited)
            {
                g_queue_push_tail (data->directories, g_object_ref (child));
            }
        }

        g_object_unref (child);
next:
        g_object_unref (info);
    }

    g_object_unref (enumerator);
}


static gpointer
search_thread_func (gpointer user_data)
{
    SearchThreadData *data;
    GFile *dir;
    GFileInfo *info;
    const char *id;

    data = user_data;
    /* Insert id for toplevel directory into visited */
    dir = g_queue_peek_head (data->directories);
    info = g_file_query_info (dir, G_FILE_ATTRIBUTE_ID_FILE, 0, data->cancellable, NULL);
    if (info)
    {
        id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
        if (id)
        {
            g_hash_table_insert (data->visited, g_strdup (id), NULL);
        }
        g_object_unref (info);
    }

    while (!g_cancellable_is_cancelled (data->cancellable) &&
           (dir = g_queue_pop_head (data->directories)) != NULL)
    {
        visit_directory (dir, data);
        g_object_unref (dir);
    }

    if (!g_cancellable_is_cancelled (data->cancellable))
    {
        send_batch_in_idle (data);
    }

    finish_search_thread (data);

    return NULL;
}

static void
create_thread_timeout (gpointer user_data)
{
    NautilusSearchEngineSimple *simple = user_data;
    g_autoptr (GThread) thread = NULL;

    simple->create_thread_timeout_id = 0;
    thread = g_thread_new ("nautilus-search-simple", search_thread_func, simple->active_search);
}

static void
nautilus_search_engine_simple_start (NautilusSearchProvider *provider)
{
    NautilusSearchEngineSimple *simple;
    SearchThreadData *data;
    g_autoptr (GFile) location = NULL;

    simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (provider);

    if (simple->active_search != NULL)
    {
        return;
    }

    g_debug ("Simple engine start");

    data = search_thread_data_new (simple, simple->query);

    simple->active_search = data;
    g_object_notify (G_OBJECT (provider), "running");

    location = nautilus_query_get_location (simple->query);
    if (location == NULL)
    {
        /* Global search. Nothing for this engine to do.*/
        finish_search_thread (data);
        return;
    }

    g_queue_push_tail (data->directories, g_steal_pointer (&location));

    simple->create_thread_timeout_id = g_timeout_add_once (CREATE_THREAD_DELAY_MS,
                                                           create_thread_timeout,
                                                           simple);
}

static void
nautilus_search_engine_simple_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineSimple *simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (provider);

    if (simple->active_search != NULL)
    {
        g_debug ("Simple engine stop");
        g_cancellable_cancel (simple->active_search->cancellable);

        if (simple->create_thread_timeout_id != 0)
        {
            /* Thread wasn't started, so we must call this directly from here.*/
            finish_search_thread (simple->active_search);

            g_clear_handle_id (&simple->create_thread_timeout_id, g_source_remove);
        }
    }
}

static void
nautilus_search_engine_simple_set_query (NautilusSearchProvider *provider,
                                         NautilusQuery          *query)
{
    NautilusSearchEngineSimple *simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (provider);

    g_clear_object (&simple->query);

    simple->query = g_object_ref (query);
}

static gboolean
nautilus_search_engine_simple_is_running (NautilusSearchProvider *provider)
{
    NautilusSearchEngineSimple *simple;

    simple = NAUTILUS_SEARCH_ENGINE_SIMPLE (provider);

    return simple->active_search != NULL;
}

static void
nautilus_search_engine_simple_get_property (GObject    *object,
                                            guint       arg_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
    NautilusSearchEngineSimple *engine = NAUTILUS_SEARCH_ENGINE_SIMPLE (object);

    switch (arg_id)
    {
        case PROP_RUNNING:
        {
            g_value_set_boolean (value, nautilus_search_engine_simple_is_running (NAUTILUS_SEARCH_PROVIDER (engine)));
        }
        break;
    }
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->set_query = nautilus_search_engine_simple_set_query;
    iface->start = nautilus_search_engine_simple_start;
    iface->stop = nautilus_search_engine_simple_stop;
    iface->is_running = nautilus_search_engine_simple_is_running;
}

static void
nautilus_search_engine_simple_class_init (NautilusSearchEngineSimpleClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;
    gobject_class->get_property = nautilus_search_engine_simple_get_property;

    /**
     * NautilusSearchEngine::running:
     *
     * Whether the search engine is running a search.
     */
    g_object_class_override_property (gobject_class, PROP_RUNNING, "running");
}

static void
nautilus_search_engine_simple_init (NautilusSearchEngineSimple *engine)
{
    engine->query = NULL;
    engine->active_search = NULL;
}

NautilusSearchEngineSimple *
nautilus_search_engine_simple_new (void)
{
    NautilusSearchEngineSimple *engine;

    engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

    return engine;
}
