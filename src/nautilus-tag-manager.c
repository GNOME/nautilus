/* nautilus-tag-manager.c
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 * Copyright (C) 2020 Sam Thursfield <sam@afuera.me.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-tag-manager.h"
#include "nautilus-file.h"
#include "nautilus-file-undo-operations.h"
#include "nautilus-file-undo-manager.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_TAG_MANAGER
#include "nautilus-debug.h"

#include <tracker-sparql.h>

#include "config.h"

struct _NautilusTagManager
{
    GObject object;

    gboolean tracker_ok;
    TrackerSparqlConnection *local;
    TrackerSparqlConnection *miner_fs;
    TrackerNotifier *notifier;

    TrackerSparqlStatement *query_starred_files;
    TrackerSparqlStatement *query_updated_file_url;

    /* Map of URI -> tracker ID */
    GHashTable *starred_file_uris;
    /* Map of tracker ID -> URI */
    GHashTable *starred_file_ids;

    GCancellable *cancellable;
};

G_DEFINE_TYPE (NautilusTagManager, nautilus_tag_manager, G_TYPE_OBJECT);

typedef enum
{
    GET_STARRED_FILES,
    GET_IDS_FOR_URLS
} OperationType;

typedef struct
{
    GTask *task;
    GList *selection;
    GHashTable *ids;
    GObject *object;
    GAsyncReadyCallback callback;
    GCancellable *cancellable;
} InsertTaskData;

typedef struct
{
    NautilusTagManager *tag_manager;
    GTask *task;
    GList *selection;
    gboolean star;
    GHashTable *ids;
} UpdateData;

enum
{
    STARRED_CHANGED,
    LAST_SIGNAL
};

#define TRACKER_MINER_FS_BUSNAME "org.freedesktop.Tracker3.Miner.Files"

#define QUERY_STARRED_FILES \
    "SELECT ?file_url ?content_id " \
    "{ " \
    "    ?content_urn nautilus:starred true . " \
    "    SERVICE <dbus:" TRACKER_MINER_FS_BUSNAME "> { " \
    "        ?content_urn nie:isStoredAs ?file_url . " \
    "        BIND (tracker:id (?content_urn) AS ?content_id) " \
    "    } " \
    "}"

#define QUERY_UPDATED_FILE_URL \
    "SELECT ?file_url EXISTS { ?content_urn nautilus:starred true } AS ?starred" \
    "{ " \
    "    SERVICE <dbus:" TRACKER_MINER_FS_BUSNAME "> { " \
    "        ?content_urn nie:isStoredAs ?file_url . " \
    "        FILTER (tracker:id(?content_urn) = ~id) " \
    "    }" \
    "}"


static guint signals[LAST_SIGNAL];

static void
destroy_insert_task_data (gpointer data)
{
    InsertTaskData *task_data;

    task_data = data;

    nautilus_file_list_free (task_data->selection);
    g_free (data);
}

static GString *
add_selection_filter (GList   *selection,
                      GString *query)
{
    NautilusFile *file;
    GList *l;

    g_string_append (query, " FILTER(?file_url IN (");

    for (l = selection; l != NULL; l = l->next)
    {
        g_autofree gchar *uri = NULL;
        g_autofree gchar *escaped_uri = NULL;

        file = l->data;

        uri = nautilus_file_get_uri (file);
        g_string_append_printf (query, "<%s>", uri);

        if (l->next != NULL)
        {
            g_string_append (query, ", ");
        }
    }

    g_string_append (query, "))");

    return query;
}

static void
start_query_or_update (TrackerSparqlConnection *local,
                       GString                 *query,
                       GAsyncReadyCallback      callback,
                       gpointer                 user_data,
                       gboolean                 is_query,
                       GCancellable            *cancellable)
{
    g_autoptr (GError) error = NULL;

    if (!local)
    {
        g_message ("nautilus-tag-manager: No Tracker connection");
        return;
    }

    if (is_query)
    {
        tracker_sparql_connection_query_async (local,
                                               query->str,
                                               cancellable,
                                               callback,
                                               user_data);
    }
    else
    {
        tracker_sparql_connection_update_async (local,
                                                query->str,
                                                G_PRIORITY_DEFAULT,
                                                cancellable,
                                                callback,
                                                user_data);
    }
}

static void
on_update_callback (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    TrackerSparqlConnection *local;
    GError *error;
    UpdateData *data;
    gint64 *new_id;
    GList *l;
    gchar *uri;

    data = user_data;

    error = NULL;

    local = TRACKER_SPARQL_CONNECTION (object);

    tracker_sparql_connection_update_finish (local, result, &error);

    if (error == NULL ||
        (error != NULL && error->code == G_IO_ERROR_CANCELLED))
    {
        for (l = data->selection; l != NULL; l = l->next)
        {
            uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

            if (data->star)
            {
                if (g_hash_table_contains (data->ids, uri))
                {
                    new_id = g_new0 (gint64, 1);

                    *new_id = (gint64) g_hash_table_lookup (data->ids, uri);
                    g_hash_table_insert (data->tag_manager->starred_file_uris,
                                         nautilus_file_get_uri (NAUTILUS_FILE (l->data)),
                                         new_id);
                    g_hash_table_insert (data->tag_manager->starred_file_ids,
                                         new_id,
                                         nautilus_file_get_uri (NAUTILUS_FILE (l->data)));
                }
                else
                {
                    g_message ("Ignoring star request for %s as we didn't get the resource ID from Tracker.", uri);
                }
            }
            else
            {
                new_id = g_hash_table_lookup (data->tag_manager->starred_file_uris, uri);
                if (new_id)
                {
                    g_hash_table_remove (data->tag_manager->starred_file_uris, uri);
                    g_hash_table_remove (data->tag_manager->starred_file_ids, new_id);
                }
            }

            g_free (uri);
        }

        if (!nautilus_file_undo_manager_is_operating ())
        {
            NautilusFileUndoInfo *undo_info;

            undo_info = nautilus_file_undo_info_starred_new (data->selection, data->star);
            nautilus_file_undo_manager_set_action (undo_info);

            g_object_unref (undo_info);
        }

        g_signal_emit_by_name (data->tag_manager, "starred-changed", nautilus_file_list_copy (data->selection));

        g_task_return_boolean (data->task, TRUE);
        g_object_unref (data->task);
    }
    else if (error && error->code == G_IO_ERROR_CANCELLED)
    {
        g_error_free (error);
    }
    else
    {
        g_warning ("error updating tags: %s", error->message);
        g_task_return_error (data->task, error);
        g_object_unref (data->task);
    }

    if (data->ids)
    {
        g_hash_table_destroy (data->ids);
    }
    nautilus_file_list_free (data->selection);
    g_free (data);
}

static gboolean
get_query_status (TrackerSparqlCursor *cursor,
                  GAsyncResult        *result,
                  OperationType        op_type,
                  gpointer             user_data)
{
    gboolean success;
    GTask *task;
    g_autoptr (GError) error = NULL;

    task = user_data;

    success = tracker_sparql_cursor_next_finish (cursor, result, &error);

    if (!success)
    {
        if (error)
        {
            g_warning ("Error on getting all tags cursor callback: %s", error->message);
        }

        g_clear_object (&cursor);

        if (error == NULL ||
            (error != NULL && error->code == G_IO_ERROR_CANCELLED))
        {
            if (op_type == GET_IDS_FOR_URLS)
            {
                g_task_return_pointer (task, g_task_get_task_data (task), NULL);
                g_object_unref (task);
            }
        }
    }

    return success;
}

/**
 * nautilus_tag_manager_get_starred_files:
 * @self: The tag manager singleton
 *
 * Returns: (element-type gchar*) (transfer container): A list of the starred urls.
 */
GList *
nautilus_tag_manager_get_starred_files (NautilusTagManager *self)
{
    return g_hash_table_get_keys (self->starred_file_uris);
}

static void
on_get_starred_files_cursor_callback (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    const gchar *url;
    gint64 *id;
    gboolean success;
    NautilusTagManager *self;
    GList *changed_files;
    NautilusFile *file;

    cursor = TRACKER_SPARQL_CURSOR (object);

    self = NAUTILUS_TAG_MANAGER (user_data);

    success = get_query_status (cursor, result, GET_STARRED_FILES, NULL);
    if (!success)
    {
        return;
    }

    id = g_new0 (gint64, 1);

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    *id = tracker_sparql_cursor_get_integer (cursor, 1);

    g_hash_table_insert (self->starred_file_uris, g_strdup (url), id);
    g_hash_table_insert (self->starred_file_ids, id, g_strdup (url));

    file = nautilus_file_get_by_uri (url);
    changed_files = g_list_prepend (NULL, file);

    g_signal_emit_by_name (self, "starred-changed", changed_files);

    nautilus_file_list_free (changed_files);

    tracker_sparql_cursor_next_async (cursor,
                                      self->cancellable,
                                      on_get_starred_files_cursor_callback,
                                      self);
}

static void
on_get_starred_files_query_callback (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    g_autoptr (GError) error = NULL;
    TrackerSparqlStatement *statement;
    NautilusTagManager *self;

    self = NAUTILUS_TAG_MANAGER (user_data);
    statement = TRACKER_SPARQL_STATEMENT (object);

    cursor = tracker_sparql_statement_execute_finish (statement,
                                                      result,
                                                      &error);

    if (error != NULL)
    {
        if (error->code != G_IO_ERROR_CANCELLED)
        {
            g_warning ("Error on getting starred files: %s", error->message);
        }
    }
    else
    {
        tracker_sparql_cursor_next_async (cursor,
                                          self->cancellable,
                                          on_get_starred_files_cursor_callback,
                                          user_data);
    }
}

static void
nautilus_tag_manager_query_starred_files (NautilusTagManager *self,
                                          GCancellable       *cancellable)
{
    if (!self->tracker_ok)
    {
        g_message ("nautilus-tag-manager: No Tracker connection");
        return;
    }

    self->cancellable = cancellable;

    tracker_sparql_statement_execute_async (self->query_starred_files,
                                            cancellable,
                                            on_get_starred_files_query_callback,
                                            self);
}

static gpointer
nautilus_tag_manager_gpointer_task_finish (GObject       *source_object,
                                           GAsyncResult  *res,
                                           GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (res, source_object), FALSE);

    return g_task_propagate_pointer (G_TASK (res), error);
}

static GString *
nautilus_tag_manager_delete_tag (NautilusTagManager *self,
                                 GList              *selection,
                                 GString            *query)
{
    g_string_append (query,
                     "DELETE { ?content_urn nautilus:starred true } "
                     "WHERE { "
                     "  SERVICE <dbus:" TRACKER_MINER_FS_BUSNAME "> { "
                     "    ?content_urn nie:isStoredAs ?file_url . ");

    query = add_selection_filter (selection, query);

    g_string_append (query, " } }\n");

    return query;
}

static GString *
nautilus_tag_manager_insert_tag (NautilusTagManager *self,
                                 GList              *selection,
                                 GString            *query)
{
    g_string_append (query,
                     "INSERT { "
                     "    ?content_urn a rdfs:Resource . "
                     "    ?content_urn nautilus:starred true . "
                     "} WHERE { "
                     "  SERVICE <dbus:" TRACKER_MINER_FS_BUSNAME "> { "
                     "    ?content_urn nie:isStoredAs ?file_url . ");

    query = add_selection_filter (selection, query);

    g_string_append (query, "} }\n");

    return query;
}

gboolean
nautilus_tag_manager_file_is_starred (NautilusTagManager *self,
                                      const gchar        *file_name)
{
    return g_hash_table_contains (self->starred_file_uris, file_name);
}

static void
on_get_file_ids_for_urls_cursor_callback (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    GTask *task;
    gint64 *id;
    const gchar *url;
    gboolean success;
    GList *l;
    gchar *file_url;
    InsertTaskData *data;

    task = user_data;
    data = g_task_get_task_data (task);

    cursor = TRACKER_SPARQL_CURSOR (object);

    success = get_query_status (cursor, result, GET_IDS_FOR_URLS, task);
    if (!success)
    {
        return;
    }

    id = g_new0 (gint64, 1);

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    *id = tracker_sparql_cursor_get_integer (cursor, 1);

    for (l = data->selection; l != NULL; l = l->next)
    {
        file_url = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

        if (g_strcmp0 (file_url, url) == 0)
        {
            g_hash_table_insert (data->ids,
                                 g_strdup (url),
                                 id);

            g_free (file_url);

            break;
        }

        g_free (file_url);
    }

    tracker_sparql_cursor_next_async (cursor,
                                      g_task_get_cancellable (task),
                                      on_get_file_ids_for_urls_cursor_callback,
                                      task);
}


static void
on_get_file_ids_for_urls_query_callback (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    g_autoptr (GError) error = NULL;
    TrackerSparqlConnection *local;
    GTask *task;

    local = TRACKER_SPARQL_CONNECTION (object);
    task = user_data;

    cursor = tracker_sparql_connection_query_finish (local, result, &error);

    if (error != NULL)
    {
        if (error->code != G_IO_ERROR_CANCELLED)
        {
            g_warning ("Error on getting id for url: %s", error->message);
            g_task_return_pointer (task, g_task_get_task_data (task), NULL);
            g_object_unref (task);
        }
    }
    else
    {
        tracker_sparql_cursor_next_async (cursor,
                                          g_task_get_cancellable (task),
                                          on_get_file_ids_for_urls_cursor_callback,
                                          user_data);
    }
}

static void
nautilus_tag_manager_get_file_ids_for_urls (NautilusTagManager *self,
                                            GList              *selection,
                                            GTask              *task)
{
    GString *query;

    query = g_string_new ("SELECT ?file_url ?content_id "
                          "WHERE { "
                          "  SERVICE <dbus:" TRACKER_MINER_FS_BUSNAME "> { "
                          "    ?content_urn nie:isStoredAs ?file_url . "
                          "    BIND (tracker:id (?content_urn) AS ?content_id) ");

    query = add_selection_filter (selection, query);

    g_string_append (query, "} }\n");

    start_query_or_update (self->local,
                           query,
                           on_get_file_ids_for_urls_query_callback,
                           task,
                           TRUE,
                           g_task_get_cancellable (task));

    g_string_free (query, TRUE);
}

static void
on_star_files_callback (GObject      *object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
    NautilusTagManager *self;
    GString *query;
    InsertTaskData *data;
    g_autoptr (GError) error = NULL;
    GTask *task;
    UpdateData *update_data;

    self = NAUTILUS_TAG_MANAGER (object);

    data = nautilus_tag_manager_gpointer_task_finish (object, res, &error);

    task = g_task_new (data->object, data->cancellable, data->callback, NULL);

    query = g_string_new ("");

    query = nautilus_tag_manager_insert_tag (self,
                                             data->selection,
                                             query);

    update_data = g_new0 (UpdateData, 1);
    update_data->task = task;
    update_data->tag_manager = self;
    update_data->selection = nautilus_file_list_copy (data->selection);
    update_data->star = TRUE;
    update_data->ids = data->ids;

    /* the ids hash table is now owned by the update_data,
     * so it will be freed by it.
     */
    destroy_insert_task_data (data);

    start_query_or_update (self->local,
                           query,
                           on_update_callback,
                           update_data,
                           FALSE,
                           g_task_get_cancellable (task));

    g_string_free (query, TRUE);
}

void
nautilus_tag_manager_star_files (NautilusTagManager  *self,
                                 GObject             *object,
                                 GList               *selection,
                                 GAsyncReadyCallback  callback,
                                 GCancellable        *cancellable)
{
    GTask *task;
    InsertTaskData *data;

    data = g_new0 (InsertTaskData, 1);
    data->selection = nautilus_file_list_copy (selection);
    data->ids = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify) g_free,
                                       (GDestroyNotify) g_free);
    data->callback = callback;
    data->object = object;
    data->cancellable = cancellable;

    DEBUG ("Starring %i files", g_list_length (selection));

    task = g_task_new (self, cancellable, on_star_files_callback, NULL);
    g_task_set_task_data (task,
                          data,
                          NULL);

    nautilus_tag_manager_get_file_ids_for_urls (self, selection, task);
}

void
nautilus_tag_manager_unstar_files (NautilusTagManager  *self,
                                   GObject             *object,
                                   GList               *selection,
                                   GAsyncReadyCallback  callback,
                                   GCancellable        *cancellable)
{
    GString *query;
    GTask *task;
    UpdateData *update_data;

    DEBUG ("Unstarring %i files", g_list_length (selection));

    task = g_task_new (object, cancellable, callback, NULL);

    query = g_string_new ("");

    query = nautilus_tag_manager_delete_tag (self,
                                             selection,
                                             query);

    update_data = g_new0 (UpdateData, 1);
    update_data->task = task;
    update_data->tag_manager = self;
    update_data->selection = nautilus_file_list_copy (selection);
    update_data->star = FALSE;

    start_query_or_update (self->local,
                           query,
                           on_update_callback,
                           update_data,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

static void
on_tracker_notifier_events (TrackerNotifier *notifier,
                            gchar           *service,
                            gchar           *graph,
                            GPtrArray       *events,
                            gpointer         user_data)
{
    TrackerNotifierEvent *event;
    NautilusTagManager *self;
    int i;
    const gchar *file_url;
    const gchar *new_file_url;
    GError *error = NULL;
    TrackerSparqlCursor *cursor;
    gboolean query_has_results = FALSE;
    gboolean is_starred;
    gint64 id, *new_id;
    GList *changed_files;
    NautilusFile *changed_file;

    self = NAUTILUS_TAG_MANAGER (user_data);

    for (i = 0; i < events->len; i++)
    {
        event = g_ptr_array_index (events, i);

        id = tracker_notifier_event_get_id (event);
        file_url = g_hash_table_lookup (self->starred_file_ids, &id);
        changed_file = NULL;

        DEBUG ("Got event for tracker resource id %li", id);

        tracker_sparql_statement_bind_int (self->query_updated_file_url, "id", tracker_notifier_event_get_id (event));
        cursor = tracker_sparql_statement_execute (self->query_updated_file_url,
                                                   NULL,
                                                   &error);

        if (cursor)
        {
            query_has_results = tracker_sparql_cursor_next (cursor, NULL, &error);
        }

        if (error || !cursor)
        {
            g_warning ("Couldn't query the Tracker Store: '%s'", error ? error->message : "(null error)");
            g_clear_error (&error);
            return;
        }

        if (!query_has_results)
        {
            if (g_hash_table_contains (self->starred_file_ids, &id))
            {
                /* The file was deleted from the filesystem or is no longer indexed by Tracker. */
                file_url = g_hash_table_lookup (self->starred_file_ids, &id);
                DEBUG ("Removing %s from starred files list, as id %li no longer present in Tracker index. ", file_url, id);

                changed_file = nautilus_file_get_by_uri (file_url);

                g_hash_table_remove (self->starred_file_ids, &id);
                g_hash_table_remove (self->starred_file_uris, file_url);
            }
        }
        else
        {
            new_file_url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
            is_starred = tracker_sparql_cursor_get_boolean (cursor, 1);

            if (g_hash_table_contains (self->starred_file_ids, &id))
            {
                if (is_starred && strcmp (file_url, new_file_url) != 0)
                {
                    new_id = g_new0 (gint64, 1);
                    *new_id = id;

                    DEBUG ("Starred file changed URI from %s to %s.", file_url, new_file_url);
                    g_hash_table_remove (self->starred_file_uris, file_url);

                    g_hash_table_insert (self->starred_file_ids, new_id, g_strdup (new_file_url));
                    g_hash_table_insert (self->starred_file_uris, g_strdup (new_file_url), new_id);

                    changed_file = nautilus_file_get_by_uri (new_file_url);
                }
                else if (!is_starred)
                {
                    DEBUG ("File is no longer starred: %s", file_url);
                    g_hash_table_remove (self->starred_file_uris, file_url);
                    g_hash_table_remove (self->starred_file_ids, &id);

                    changed_file = nautilus_file_get_by_uri (new_file_url);
                }
            }
            else if (is_starred)
            {
                DEBUG ("File is now starred: %s", new_file_url);
                new_id = g_new0 (gint64, 1);
                *new_id = id;

                g_hash_table_insert (self->starred_file_ids, new_id, g_strdup (new_file_url));
                g_hash_table_insert (self->starred_file_uris, g_strdup (new_file_url), new_id);

                changed_file = nautilus_file_get_by_uri (new_file_url);
            }
        }

        if (changed_file)
        {
            changed_files = g_list_prepend (NULL, changed_file);

            g_signal_emit_by_name (self, "starred-changed", changed_files);

            nautilus_file_list_free (changed_files);
        }

        g_object_unref (cursor);
    }
}

static void
nautilus_tag_manager_finalize (GObject *object)
{
    NautilusTagManager *self;

    self = NAUTILUS_TAG_MANAGER (object);

    if (self->notifier != NULL)
    {
        g_signal_handlers_disconnect_by_func (self->notifier,
                                              G_CALLBACK (on_tracker_notifier_events),
                                              self);
    }

    g_clear_object (&self->notifier);
    g_clear_object (&self->local);
    g_clear_object (&self->miner_fs);
    g_clear_object (&self->query_updated_file_url);
    g_clear_object (&self->query_starred_files);

    g_hash_table_destroy (self->starred_file_ids);
    g_hash_table_destroy (self->starred_file_uris);

    G_OBJECT_CLASS (nautilus_tag_manager_parent_class)->finalize (object);
}

static void
nautilus_tag_manager_class_init (NautilusTagManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_tag_manager_finalize;

    signals[STARRED_CHANGED] = g_signal_new ("starred-changed",
                                             NAUTILUS_TYPE_TAG_MANAGER,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL,
                                             NULL,
                                             g_cclosure_marshal_VOID__POINTER,
                                             G_TYPE_NONE,
                                             1,
                                             G_TYPE_POINTER);
}

NautilusTagManager *
nautilus_tag_manager_get (void)
{
    static NautilusTagManager *tag_manager = NULL;

    if (tag_manager != NULL)
    {
        return g_object_ref (tag_manager);
    }

    tag_manager = g_object_new (NAUTILUS_TYPE_TAG_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (tag_manager), (gpointer) & tag_manager);

    return tag_manager;
}

static gboolean
setup_tracker_connections (NautilusTagManager  *self,
                           GCancellable        *cancellable,
                           GError             **error)
{
    const gchar *datadir;
    g_autofree gchar *store_path = NULL;
    g_autofree gchar *ontology_path = NULL;
    g_autoptr (GFile) store = NULL;
    g_autoptr (GFile) ontology = NULL;

    /* Connect to private database to store nautilus:starred property. */

    if (g_getenv ("NAUTILUS_TEST_DATADIR"))
    {
        datadir = g_getenv ("NAUTILUS_TEST_DATADIR");
    }
    else
    {
        datadir = NAUTILUS_DATADIR;
    }

    store_path = g_build_filename (g_get_user_data_dir (), "nautilus", NULL);
    ontology_path = g_build_filename (datadir, "ontology", NULL);

    store = g_file_new_for_path (store_path);
    ontology = g_file_new_for_path (ontology_path);

    self->local = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                                 store,
                                                 ontology,
                                                 cancellable,
                                                 error);

    if (*error)
    {
        return FALSE;
    }

    /* Connect to Tracker filesystem index to follow file renames. */
    self->miner_fs = tracker_sparql_connection_bus_new (TRACKER_MINER_FS_BUSNAME,
                                                        NULL,
                                                        NULL,
                                                        error);

    if (*error)
    {
        return FALSE;
    }

    /* Prepare reusable queries. */
    self->query_updated_file_url = tracker_sparql_connection_query_statement (self->local,
                                                                              QUERY_UPDATED_FILE_URL,
                                                                              cancellable,
                                                                              error);

    if (*error)
    {
        return FALSE;
    }

    self->query_starred_files = tracker_sparql_connection_query_statement (self->local,
                                                                           QUERY_STARRED_FILES,
                                                                           cancellable,
                                                                           error);

    if (*error)
    {
        return FALSE;
    }

    return TRUE;
}

/* Initialize the tag mananger. */
void
nautilus_tag_manager_set_cancellable (NautilusTagManager *self,
                                      GCancellable       *cancellable)
{
    g_autoptr (GError) error = NULL;

    self->tracker_ok = setup_tracker_connections (self, cancellable, &error);

    if (error)
    {
        g_warning ("Unable to initialize tag manager: %s", error->message);
        return;
    }

    self->notifier = tracker_sparql_connection_create_notifier (self->miner_fs,
                                                                TRACKER_NOTIFIER_FLAG_NONE);

    nautilus_tag_manager_query_starred_files (self, cancellable);

    g_signal_connect (self->notifier,
                      "events",
                      G_CALLBACK (on_tracker_notifier_events),
                      self);
}

static void
nautilus_tag_manager_init (NautilusTagManager *self)
{
    self->starred_file_uris = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     (GDestroyNotify) g_free,
                                                     /* values are keys in the other hash table
                                                      * and are freed there */
                                                     NULL);
    self->starred_file_ids = g_hash_table_new_full (g_int_hash,
                                                    g_int_equal,
                                                    (GDestroyNotify) g_free,
                                                    (GDestroyNotify) g_free);
}
