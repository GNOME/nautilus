/*
 *  Copyright (C) 2005 Novell, Inc
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Anders Carlsson <andersca@imendio.com>
 */

#include "nautilus-search-directory.h"

#include <eel/eel-glib-extensions.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <sys/time.h>

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-query.h"
#include "nautilus-search-directory-file.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-search-engine.h"
#include "nautilus-search-provider.h"

struct _NautilusSearchDirectory
{
    NautilusDirectory parent_instance;

    NautilusQuery *query;

    NautilusSearchEngine *engine;

    gboolean search_running;
    /* When the search directory is stopped or cancelled, we migth wait
     * until all data and signals from previous search are stopped and removed
     * from the search engine. While this situation happens we don't want to connect
     * clients to our signals, and we will wait until the search data and signals
     * are valid and ready.
     * The worst thing that can happens if we don't do this is that new clients
     * migth get the information of old searchs if they are waiting_for_file_list.
     * But that shouldn't be a big deal since old clients have the old information.
     * But anyway it's currently unused for this case since the only client is
     * nautilus-view and is not waiting_for_file_list :) .
     *
     * The other use case is for letting clients know if information of the directory
     * is outdated or not valid. This might happens for automatic
     * scheduled timeouts. */
    gboolean search_ready_and_valid;

    guint hits_queue_process_idle_id;
    gboolean hits_queue_emit_done;
    GList *hits_queue;

    GList *files;
    GHashTable *files_hash;

    GList *monitor_list;
    GList *callback_list;
    GList *pending_callback_list;

    GBinding *binding;

    NautilusDirectory *base_model;
};

typedef struct
{
    gboolean monitor_hidden_files;
    NautilusFileAttributes monitor_attributes;

    gconstpointer client;
} SearchMonitor;

typedef struct
{
    NautilusSearchDirectory *search_directory;

    NautilusDirectoryCallback callback;
    gpointer callback_data;

    NautilusFileAttributes wait_for_attributes;
    gboolean wait_for_file_list;
    GList *file_list;
    GHashTable *non_ready_hash;
} SearchCallback;

enum
{
    PROP_0,
    PROP_BASE_MODEL,
    PROP_QUERY,
    NUM_PROPERTIES
};

G_DEFINE_TYPE_WITH_CODE (NautilusSearchDirectory, nautilus_search_directory, NAUTILUS_TYPE_DIRECTORY,
                         nautilus_ensure_extension_points ();
                         /* It looks like you’re implementing an extension point.
                          * Did you modify nautilus_ensure_extension_builtins() accordingly?
                          *
                          * • Yes
                          * • Doing it right now
                          */
                         g_io_extension_point_implement (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         NAUTILUS_SEARCH_DIRECTORY_PROVIDER_NAME,
                                                         0));

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void search_engine_hits_added (NautilusSearchEngine    *engine,
                                      GList                   *hits,
                                      NautilusSearchDirectory *self);
static void search_engine_error (NautilusSearchEngine    *engine,
                                 const char              *error,
                                 NautilusSearchDirectory *self);
static void search_callback_file_ready_callback (NautilusFile *file,
                                                 gpointer      data);
static void file_changed (NautilusFile            *file,
                          NautilusSearchDirectory *self);

static void
reset_hits_queue (NautilusSearchDirectory *self)
{
    if (self->hits_queue != NULL)
    {
        g_list_free_full (self->hits_queue, g_object_unref);
        self->hits_queue = NULL;
    }

    self->hits_queue_emit_done = FALSE;
}

static void
reset_file_list (NautilusSearchDirectory *self)
{
    GList *list, *monitor_list;
    NautilusFile *file;
    SearchMonitor *monitor;

    /* Remove file connections */
    for (list = self->files; list != NULL; list = list->next)
    {
        file = list->data;

        /* Disconnect change handler */
        g_signal_handlers_disconnect_by_func (file, file_changed, self);

        /* Remove monitors */
        for (monitor_list = self->monitor_list; monitor_list;
             monitor_list = monitor_list->next)
        {
            monitor = monitor_list->data;
            nautilus_file_monitor_remove (file, monitor);
        }
    }

    nautilus_file_list_free (self->files);
    self->files = NULL;

    g_hash_table_remove_all (self->files_hash);
}

static void
set_hidden_files (NautilusSearchDirectory *self)
{
    GList *l;
    SearchMonitor *monitor;
    gboolean monitor_hidden = FALSE;

    for (l = self->monitor_list; l != NULL; l = l->next)
    {
        monitor = l->data;
        monitor_hidden |= monitor->monitor_hidden_files;

        if (monitor_hidden)
        {
            break;
        }
    }

    nautilus_query_set_show_hidden_files (self->query, monitor_hidden);
}

static void
start_search (NautilusSearchDirectory *self)
{
    NautilusSearchEngineModel *model_provider;

    if (!self->query)
    {
        return;
    }

    if (self->search_running)
    {
        return;
    }

    if (!self->monitor_list && !self->pending_callback_list)
    {
        return;
    }

    /* We need to start the search engine */
    self->search_running = TRUE;
    self->search_ready_and_valid = FALSE;

    set_hidden_files (self);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (self->engine),
                                        self->query);

    model_provider = nautilus_search_engine_get_model_provider (self->engine);
    nautilus_search_engine_model_set_model (model_provider, self->base_model);

    reset_hits_queue (self);
    reset_file_list (self);

    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (self->engine));
}

static void
stop_search (NautilusSearchDirectory *self)
{
    if (!self->search_running)
    {
        return;
    }

    self->search_running = FALSE;
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (self->engine));

    reset_hits_queue (self);
    reset_file_list (self);
}

static void
file_changed (NautilusFile            *file,
              NautilusSearchDirectory *self)
{
    GList list;

    list.data = file;
    list.next = NULL;

    nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (self), &list);
}

static void
search_monitor_add (NautilusDirectory         *directory,
                    gconstpointer              client,
                    gboolean                   monitor_hidden_files,
                    NautilusFileAttributes     file_attributes,
                    NautilusDirectoryCallback  callback,
                    gpointer                   callback_data)
{
    GList *list;
    SearchMonitor *monitor;
    NautilusSearchDirectory *self;
    NautilusFile *file;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);

    monitor = g_new0 (SearchMonitor, 1);
    monitor->monitor_hidden_files = monitor_hidden_files;
    monitor->monitor_attributes = file_attributes;
    monitor->client = client;

    self->monitor_list = g_list_prepend (self->monitor_list, monitor);

    if (callback != NULL)
    {
        (*callback)(directory, self->files, callback_data);
    }

    for (list = self->files; list != NULL; list = list->next)
    {
        file = list->data;

        /* Add monitors */
        nautilus_file_monitor_add (file, monitor, file_attributes);
    }

    start_search (self);
}

static void
search_monitor_remove_file_monitors (SearchMonitor           *monitor,
                                     NautilusSearchDirectory *self)
{
    GList *list;
    NautilusFile *file;

    for (list = self->files; list != NULL; list = list->next)
    {
        file = list->data;

        nautilus_file_monitor_remove (file, monitor);
    }
}

static void
search_monitor_destroy (SearchMonitor           *monitor,
                        NautilusSearchDirectory *self)
{
    search_monitor_remove_file_monitors (monitor, self);

    g_free (monitor);
}

static void
search_monitor_remove (NautilusDirectory *directory,
                       gconstpointer      client)
{
    NautilusSearchDirectory *self;
    SearchMonitor *monitor;
    GList *list;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);

    for (list = self->monitor_list; list != NULL; list = list->next)
    {
        monitor = list->data;

        if (monitor->client == client)
        {
            self->monitor_list = g_list_delete_link (self->monitor_list, list);

            search_monitor_destroy (monitor, self);

            break;
        }
    }

    if (!self->monitor_list)
    {
        stop_search (self);
    }
}

static void
cancel_call_when_ready (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
    SearchCallback *search_callback;
    NautilusFile *file;

    file = key;
    search_callback = user_data;

    nautilus_file_cancel_call_when_ready (file, search_callback_file_ready_callback,
                                          search_callback);
}

static void
search_callback_destroy (SearchCallback *search_callback)
{
    if (search_callback->non_ready_hash)
    {
        g_hash_table_foreach (search_callback->non_ready_hash, cancel_call_when_ready, search_callback);
        g_hash_table_destroy (search_callback->non_ready_hash);
    }

    nautilus_file_list_free (search_callback->file_list);

    g_free (search_callback);
}

static void
search_callback_invoke_and_destroy (SearchCallback *search_callback)
{
    search_callback->callback (NAUTILUS_DIRECTORY (search_callback->search_directory),
                               search_callback->file_list,
                               search_callback->callback_data);

    search_callback->search_directory->callback_list =
        g_list_remove (search_callback->search_directory->callback_list, search_callback);

    search_callback_destroy (search_callback);
}

static void
search_callback_file_ready_callback (NautilusFile *file,
                                     gpointer      data)
{
    SearchCallback *search_callback = data;

    g_hash_table_remove (search_callback->non_ready_hash, file);

    if (g_hash_table_size (search_callback->non_ready_hash) == 0)
    {
        search_callback_invoke_and_destroy (search_callback);
    }
}

static void
search_callback_add_file_callbacks (SearchCallback *callback)
{
    GList *file_list_copy, *list;
    NautilusFile *file;

    file_list_copy = g_list_copy (callback->file_list);

    for (list = file_list_copy; list != NULL; list = list->next)
    {
        file = list->data;

        nautilus_file_call_when_ready (file,
                                       callback->wait_for_attributes,
                                       search_callback_file_ready_callback,
                                       callback);
    }
    g_list_free (file_list_copy);
}

static SearchCallback *
search_callback_find (NautilusSearchDirectory   *self,
                      NautilusDirectoryCallback  callback,
                      gpointer                   callback_data)
{
    SearchCallback *search_callback;
    GList *list;

    for (list = self->callback_list; list != NULL; list = list->next)
    {
        search_callback = list->data;

        if (search_callback->callback == callback &&
            search_callback->callback_data == callback_data)
        {
            return search_callback;
        }
    }

    return NULL;
}

static SearchCallback *
search_callback_find_pending (NautilusSearchDirectory   *self,
                              NautilusDirectoryCallback  callback,
                              gpointer                   callback_data)
{
    SearchCallback *search_callback;
    GList *list;

    for (list = self->pending_callback_list; list != NULL; list = list->next)
    {
        search_callback = list->data;

        if (search_callback->callback == callback &&
            search_callback->callback_data == callback_data)
        {
            return search_callback;
        }
    }

    return NULL;
}

static GHashTable *
file_list_to_hash_table (GList *file_list)
{
    GList *list;
    GHashTable *table;

    if (!file_list)
    {
        return NULL;
    }

    table = g_hash_table_new (NULL, NULL);

    for (list = file_list; list != NULL; list = list->next)
    {
        g_hash_table_insert (table, list->data, list->data);
    }

    return table;
}

static void
search_call_when_ready (NautilusDirectory         *directory,
                        NautilusFileAttributes     file_attributes,
                        gboolean                   wait_for_file_list,
                        NautilusDirectoryCallback  callback,
                        gpointer                   callback_data)
{
    NautilusSearchDirectory *self;
    SearchCallback *search_callback;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);

    search_callback = search_callback_find (self, callback, callback_data);
    if (search_callback == NULL)
    {
        search_callback = search_callback_find_pending (self, callback, callback_data);
    }

    if (search_callback)
    {
        g_warning ("tried to add a new callback while an old one was pending");
        return;
    }

    search_callback = g_new0 (SearchCallback, 1);
    search_callback->search_directory = self;
    search_callback->callback = callback;
    search_callback->callback_data = callback_data;
    search_callback->wait_for_attributes = file_attributes;
    search_callback->wait_for_file_list = wait_for_file_list;

    if (wait_for_file_list && !self->search_ready_and_valid)
    {
        /* Add it to the pending callback list, which will be
         * processed when the directory has valid data from the new
         * search and all data and signals from previous searchs is removed. */
        self->pending_callback_list =
            g_list_prepend (self->pending_callback_list, search_callback);

        /* We might need to start the search engine */
        start_search (self);
    }
    else
    {
        search_callback->file_list = nautilus_file_list_copy (self->files);
        search_callback->non_ready_hash = file_list_to_hash_table (self->files);

        if (!search_callback->non_ready_hash)
        {
            /* If there are no ready files, we invoke the callback
             *  with an empty list.
             */
            search_callback_invoke_and_destroy (search_callback);
        }
        else
        {
            self->callback_list = g_list_prepend (self->callback_list, search_callback);
            search_callback_add_file_callbacks (search_callback);
        }
    }
}

static void
search_cancel_callback (NautilusDirectory         *directory,
                        NautilusDirectoryCallback  callback,
                        gpointer                   callback_data)
{
    NautilusSearchDirectory *self;
    SearchCallback *search_callback;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);
    search_callback = search_callback_find (self, callback, callback_data);

    if (search_callback)
    {
        self->callback_list = g_list_remove (self->callback_list, search_callback);

        search_callback_destroy (search_callback);

        goto done;
    }

    /* Check for a pending callback */
    search_callback = search_callback_find_pending (self, callback, callback_data);

    if (search_callback)
    {
        self->pending_callback_list = g_list_remove (self->pending_callback_list, search_callback);

        search_callback_destroy (search_callback);
    }

done:
    if (!self->callback_list && !self->pending_callback_list)
    {
        stop_search (self);
    }
}

static void
search_callback_add_pending_file_callbacks (SearchCallback *callback)
{
    callback->file_list = nautilus_file_list_copy (callback->search_directory->files);
    callback->non_ready_hash = file_list_to_hash_table (callback->search_directory->files);

    search_callback_add_file_callbacks (callback);
}

static void
search_directory_add_pending_files_callbacks (NautilusSearchDirectory *self)
{
    /* Add all file callbacks */
    g_list_foreach (self->pending_callback_list,
                    (GFunc) search_callback_add_pending_file_callbacks, NULL);
    self->callback_list = g_list_concat (self->callback_list,
                                         self->pending_callback_list);

    g_list_free (self->pending_callback_list);
    self->pending_callback_list = NULL;
}

static void
on_search_directory_search_ready_and_valid (NautilusSearchDirectory *self)
{
    search_directory_add_pending_files_callbacks (self);
    self->search_ready_and_valid = TRUE;
}

static gboolean
hits_queue_process_idle (gpointer user_data)
{
    NautilusSearchDirectory *self = user_data;
    GList *file_list = NULL;
    NautilusFile *file;
    SearchMonitor *monitor;
    GList *monitor_list;

    NautilusSearchHit *hit;
    const char *uri;

    if (self->hits_queue == NULL)
    {
        if (self->hits_queue_emit_done)
        {
            on_search_directory_search_ready_and_valid (self);
            nautilus_directory_emit_done_loading (NAUTILUS_DIRECTORY (self));
            self->hits_queue_emit_done = FALSE;
        }
        self->hits_queue_process_idle_id = 0;
        return FALSE;
    }

    hit = self->hits_queue->data;
    self->hits_queue = g_list_delete_link (self->hits_queue, self->hits_queue);

    uri = nautilus_search_hit_get_uri (hit);

    nautilus_search_hit_compute_scores (hit, self->query);

    file = nautilus_file_get_by_uri (uri);
    nautilus_file_set_search_relevance (file, nautilus_search_hit_get_relevance (hit));
    nautilus_file_set_search_fts_snippet (file, nautilus_search_hit_get_fts_snippet (hit));

    for (monitor_list = self->monitor_list; monitor_list; monitor_list = monitor_list->next)
    {
        monitor = monitor_list->data;

        /* Add monitors */
        nautilus_file_monitor_add (file, monitor, monitor->monitor_attributes);
    }

    g_signal_connect (file, "changed", G_CALLBACK (file_changed), self),

    file_list = g_list_prepend (file_list, file);
    g_hash_table_add (self->files_hash, file);

    self->files = g_list_concat (self->files, file_list);

    nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (self), file_list);

    file = nautilus_directory_get_corresponding_file (NAUTILUS_DIRECTORY (self));
    nautilus_file_emit_changed (file);
    nautilus_file_unref (file);

    search_directory_add_pending_files_callbacks (self);

    g_object_unref (hit);

    return TRUE;
}

static void
search_engine_hits_added (NautilusSearchEngine    *engine,
                          GList                   *hits,
                          NautilusSearchDirectory *self)
{
    if (hits != NULL)
    {
        /* Delegate potentially large hits to an idle processor */
        self->hits_queue = g_list_concat (
            self->hits_queue,
            g_list_copy_deep (hits, (GCopyFunc) g_object_ref, NULL));
        if (self->hits_queue_process_idle_id == 0)
        {
            self->hits_queue_process_idle_id = g_idle_add (hits_queue_process_idle, self);
        }
    }
}

static void
search_engine_error (NautilusSearchEngine    *engine,
                     const char              *error_message,
                     NautilusSearchDirectory *self)
{
    GError *error;

    reset_hits_queue (self);
    error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                                 error_message);
    nautilus_directory_emit_load_error (NAUTILUS_DIRECTORY (self),
                                        error);
    g_error_free (error);
}

static void
search_engine_finished (NautilusSearchEngine         *engine,
                        NautilusSearchProviderStatus  status,
                        NautilusSearchDirectory      *self)
{
    /* If the search engine is going to restart means it finished an old search
     * that was stopped or cancelled.
     * Don't emit the done loading signal in this case, since this means the search
     * directory tried to start a new search before all the search providers were finished
     * in the search engine.
     * If we emit the done-loading signal in this situation the client will think
     * that it finished the current search, not an old one like it's actually
     * happening. */
    if (status == NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL)
    {
        if (self->hits_queue_process_idle_id == 0)
        {
            on_search_directory_search_ready_and_valid (self);
            nautilus_directory_emit_done_loading (NAUTILUS_DIRECTORY (self));
        }
        else
        {
            self->hits_queue_emit_done = TRUE;
        }
    }
    else if (status == NAUTILUS_SEARCH_PROVIDER_STATUS_RESTARTING)
    {
        /* Remove file monitors of the files from an old search that just
         * actually finished */
        reset_hits_queue (self);
        reset_file_list (self);
    }
}

static void
search_force_reload (NautilusDirectory *directory)
{
    NautilusSearchDirectory *self;
    NautilusFile *file;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);

    if (!self->query)
    {
        return;
    }

    self->search_ready_and_valid = FALSE;

    /* Remove file monitors */
    reset_hits_queue (self);
    reset_file_list (self);
    stop_search (self);

    file = nautilus_directory_get_corresponding_file (directory);
    nautilus_file_invalidate_all_attributes (file);
    nautilus_file_unref (file);
}

static gboolean
search_are_all_files_seen (NautilusDirectory *directory)
{
    NautilusSearchDirectory *self;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);

    return (!self->query ||
            self->search_ready_and_valid);
}

static gboolean
search_contains_file (NautilusDirectory *directory,
                      NautilusFile      *file)
{
    NautilusSearchDirectory *self;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);
    return (g_hash_table_lookup (self->files_hash, file) != NULL);
}

static GList *
search_get_file_list (NautilusDirectory *directory)
{
    NautilusSearchDirectory *self;

    self = NAUTILUS_SEARCH_DIRECTORY (directory);

    return nautilus_file_list_copy (self->files);
}


static gboolean
search_is_editable (NautilusDirectory *directory)
{
    return FALSE;
}

static gboolean
real_handles_location (GFile *location)
{
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (location);

    return eel_uri_is_search (uri);
}

static void
search_set_property (GObject      *object,
                     guint         property_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
    NautilusSearchDirectory *self = NAUTILUS_SEARCH_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_BASE_MODEL:
        {
            nautilus_search_directory_set_base_model (self, g_value_get_object (value));
        }
        break;

        case PROP_QUERY:
        {
            nautilus_search_directory_set_query (self, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
search_get_property (GObject    *object,
                     guint       property_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
    NautilusSearchDirectory *self = NAUTILUS_SEARCH_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_BASE_MODEL:
        {
            g_value_set_object (value, nautilus_search_directory_get_base_model (self));
        }
        break;

        case PROP_QUERY:
        {
            g_value_take_object (value, nautilus_search_directory_get_query (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
clear_base_model (NautilusSearchDirectory *self)
{
    if (self->base_model != NULL)
    {
        nautilus_directory_file_monitor_remove (self->base_model,
                                                &self->base_model);
        g_clear_object (&self->base_model);
    }
}

static void
search_connect_engine (NautilusSearchDirectory *self)
{
    g_signal_connect (self->engine, "hits-added",
                      G_CALLBACK (search_engine_hits_added),
                      self);
    g_signal_connect (self->engine, "error",
                      G_CALLBACK (search_engine_error),
                      self);
    g_signal_connect (self->engine, "finished",
                      G_CALLBACK (search_engine_finished),
                      self);
}

static void
search_disconnect_engine (NautilusSearchDirectory *self)
{
    g_signal_handlers_disconnect_by_func (self->engine,
                                          search_engine_hits_added,
                                          self);
    g_signal_handlers_disconnect_by_func (self->engine,
                                          search_engine_error,
                                          self);
    g_signal_handlers_disconnect_by_func (self->engine,
                                          search_engine_finished,
                                          self);
}

static void
search_dispose (GObject *object)
{
    NautilusSearchDirectory *self;
    GList *list;

    self = NAUTILUS_SEARCH_DIRECTORY (object);

    clear_base_model (self);

    /* Remove search monitors */
    if (self->monitor_list)
    {
        for (list = self->monitor_list; list != NULL; list = list->next)
        {
            search_monitor_destroy ((SearchMonitor *) list->data, self);
        }

        g_list_free (self->monitor_list);
        self->monitor_list = NULL;
    }

    if (self->hits_queue_process_idle_id > 0)
    {
        g_source_remove(self->hits_queue_process_idle_id);
        self->hits_queue_process_idle_id = 0;
    }
    self->hits_queue_emit_done = FALSE;
    reset_hits_queue (self);

    reset_file_list (self);

    if (self->callback_list)
    {
        /* Remove callbacks */
        g_list_foreach (self->callback_list,
                        (GFunc) search_callback_destroy, NULL);
        g_list_free (self->callback_list);
        self->callback_list = NULL;
    }

    if (self->pending_callback_list)
    {
        g_list_foreach (self->pending_callback_list,
                        (GFunc) search_callback_destroy, NULL);
        g_list_free (self->pending_callback_list);
        self->pending_callback_list = NULL;
    }

    g_clear_object (&self->query);
    stop_search (self);
    search_disconnect_engine (self);

    g_clear_object (&self->engine);

    G_OBJECT_CLASS (nautilus_search_directory_parent_class)->dispose (object);
}

static void
search_finalize (GObject *object)
{
    NautilusSearchDirectory *self;

    self = NAUTILUS_SEARCH_DIRECTORY (object);

    g_hash_table_destroy (self->files_hash);

    G_OBJECT_CLASS (nautilus_search_directory_parent_class)->finalize (object);
}

static void
nautilus_search_directory_init (NautilusSearchDirectory *self)
{
    self->query = NULL;
    self->hits_queue_process_idle_id = 0;
    self->hits_queue = NULL;
    self->files_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

    self->engine = nautilus_search_engine_new ();
    search_connect_engine (self);
}

static void
nautilus_search_directory_class_init (NautilusSearchDirectoryClass *class)
{
    NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (class);
    GObjectClass *oclass = G_OBJECT_CLASS (class);

    oclass->dispose = search_dispose;
    oclass->finalize = search_finalize;
    oclass->get_property = search_get_property;
    oclass->set_property = search_set_property;

    directory_class->are_all_files_seen = search_are_all_files_seen;
    directory_class->contains_file = search_contains_file;
    directory_class->force_reload = search_force_reload;
    directory_class->call_when_ready = search_call_when_ready;
    directory_class->cancel_callback = search_cancel_callback;

    directory_class->file_monitor_add = search_monitor_add;
    directory_class->file_monitor_remove = search_monitor_remove;

    directory_class->get_file_list = search_get_file_list;
    directory_class->is_editable = search_is_editable;
    directory_class->handles_location = real_handles_location;

    properties[PROP_BASE_MODEL] =
        g_param_spec_object ("base-model",
                             "The base model",
                             "The base directory model for this directory",
                             NAUTILUS_TYPE_DIRECTORY,
                             G_PARAM_READWRITE);
    properties[PROP_QUERY] =
        g_param_spec_object ("query",
                             "The query",
                             "The query for this search directory",
                             NAUTILUS_TYPE_QUERY,
                             G_PARAM_READWRITE);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_search_directory_set_base_model (NautilusSearchDirectory *self,
                                          NautilusDirectory       *base_model)
{
    if (self->base_model == base_model)
    {
        return;
    }

    if (self->query != NULL)
    {
        GFile *query_location, *model_location;
        gboolean is_equal;

        query_location = nautilus_query_get_location (self->query);
        model_location = nautilus_directory_get_location (base_model);

        is_equal = g_file_equal (model_location, query_location);

        g_object_unref (model_location);
        g_object_unref (query_location);

        if (!is_equal)
        {
            return;
        }
    }

    clear_base_model (self);
    self->base_model = nautilus_directory_ref (base_model);

    if (self->base_model != NULL)
    {
        nautilus_directory_file_monitor_add (base_model, &self->base_model,
                                             TRUE, NAUTILUS_FILE_ATTRIBUTE_INFO,
                                             NULL, NULL);
    }

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BASE_MODEL]);
}

NautilusDirectory *
nautilus_search_directory_get_base_model (NautilusSearchDirectory *self)
{
    return self->base_model;
}

char *
nautilus_search_directory_generate_new_uri (void)
{
    static int counter = 0;
    char *uri;

    uri = g_strdup_printf (EEL_SEARCH_URI "//%d/", counter++);

    return uri;
}

void
nautilus_search_directory_set_query (NautilusSearchDirectory *self,
                                     NautilusQuery           *query)
{
    NautilusFile *file;
    NautilusQuery *old_query;

    old_query = self->query;

    if (self->query != query)
    {
        self->query = g_object_ref (query);

        g_clear_pointer (&self->binding, g_binding_unbind);

        if (query)
        {
            self->binding = g_object_bind_property (self->engine, "running",
                                                    query, "searching",
                                                    G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
        }

        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_QUERY]);

        g_clear_object (&old_query);
    }

    file = nautilus_directory_get_existing_corresponding_file (NAUTILUS_DIRECTORY (self));
    if (file != NULL)
    {
        nautilus_search_directory_file_update_display_name (NAUTILUS_SEARCH_DIRECTORY_FILE (file));
    }
    nautilus_file_unref (file);
}

NautilusQuery *
nautilus_search_directory_get_query (NautilusSearchDirectory *self)
{
    if (self->query != NULL)
    {
        return g_object_ref (self->query);
    }

    return NULL;
}
