/* nautilus-tag-manager.c
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
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

#include <tracker-sparql.h>

struct _NautilusTagManager
{
    GObject object;

    TrackerNotifier *notifier;
    GError *notifier_error;

    GHashTable *starred_files;
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

#define STARRED_TAG "<urn:gnome:nautilus:starred>"

static guint signals[LAST_SIGNAL];

static const gchar *
nautilus_tag_manager_file_with_id_changed_url (GHashTable  *hash_table,
                                               gint64       id,
                                               const gchar *url)
{
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, hash_table);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        if ((gint64) value == id && g_strcmp0 (url, key) != 0)
        {
            return key;
        }
    }

    return NULL;
}

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

    g_string_append (query, " FILTER(?url IN (");

    for (l = selection; l != NULL; l = l->next)
    {
        g_autofree gchar *uri = NULL;
        g_autofree gchar *escaped_uri = NULL;

        file = l->data;

        uri = nautilus_file_get_uri (file);
        escaped_uri = tracker_sparql_escape_string (uri);
        g_string_append_printf (query, "'%s'", escaped_uri);

        if (l->next != NULL)
        {
            g_string_append (query, ", ");
        }
    }

    g_string_append (query, "))");

    return query;
}

static void
start_query_or_update (GString             *query,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data,
                       gboolean             is_query,
                       GCancellable        *cancellable)
{
    g_autoptr (GError) error = NULL;
    TrackerSparqlConnection *connection;

    connection = tracker_sparql_connection_get (cancellable, &error);
    if (!connection)
    {
        if (error)
        {
            g_warning ("Error on getting connection: %s", error->message);
        }

        return;
    }

    if (is_query)
    {
        tracker_sparql_connection_query_async (connection,
                                               query->str,
                                               cancellable,
                                               callback,
                                               user_data);
    }
    else
    {
        tracker_sparql_connection_update_async (connection,
                                                query->str,
                                                G_PRIORITY_DEFAULT,
                                                cancellable,
                                                callback,
                                                user_data);
    }

    g_object_unref (connection);
}

static void
on_query_callback (GObject             *object,
                   GAsyncResult        *result,
                   gpointer             user_data,
                   GAsyncReadyCallback  callback,
                   OperationType        op_type,
                   GCancellable        *cancellable)
{
    TrackerSparqlCursor *cursor;
    g_autoptr (GError) error = NULL;
    TrackerSparqlConnection *connection;
    GTask *task;

    task = user_data;

    connection = TRACKER_SPARQL_CONNECTION (object);

    cursor = tracker_sparql_connection_query_finish (connection,
                                                     result,
                                                     &error);

    if (error != NULL)
    {
        if (error->code != G_IO_ERROR_CANCELLED)
        {
            if (op_type == GET_STARRED_FILES)
            {
                g_warning ("Error on getting starred files: %s", error->message);
            }
            else if (op_type == GET_IDS_FOR_URLS)
            {
                g_warning ("Error on getting id for url: %s", error->message);
                g_task_return_pointer (task, g_task_get_task_data (task), NULL);
                g_object_unref (task);
            }
            else
            {
                g_warning ("Error on getting query callback: %s", error->message);
            }
        }
    }
    else
    {
        tracker_sparql_cursor_next_async (cursor,
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
    TrackerSparqlConnection *connection;
    GError *error;
    UpdateData *data;
    gint64 *id;
    GList *l;
    gchar *uri;

    data = user_data;

    error = NULL;

    connection = TRACKER_SPARQL_CONNECTION (object);

    tracker_sparql_connection_update_finish (connection, result, &error);

    if (error == NULL ||
        (error != NULL && error->code != G_IO_ERROR_CANCELLED))
    {
        for (l = data->selection; l != NULL; l = l->next)
        {
            uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

            if (data->star)
            {
                if (g_hash_table_contains (data->ids, uri))
                {
                    id = g_new0 (gint64, 1);

                    *id = (gint64) g_hash_table_lookup (data->ids, uri);
                    g_hash_table_insert (data->tag_manager->starred_files,
                                         nautilus_file_get_uri (NAUTILUS_FILE (l->data)),
                                         id);
                }
            }
            else
            {
                g_hash_table_remove (data->tag_manager->starred_files, uri);
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
        g_task_return_error (data->task, error);
        g_object_unref (data->task);
        g_warning ("error updating tags: %s", error->message);
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
            (error != NULL && error->code != G_IO_ERROR_CANCELLED))
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
    return g_hash_table_get_keys (self->starred_files);
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

    g_hash_table_insert (self->starred_files,
                         g_strdup (url),
                         id);

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
    NautilusTagManager *self;

    self = NAUTILUS_TAG_MANAGER (user_data);

    on_query_callback (object,
                       result,
                       user_data,
                       on_get_starred_files_cursor_callback,
                       GET_STARRED_FILES,
                       self->cancellable);
}

static void
nautilus_tag_manager_query_starred_files (NautilusTagManager *self,
                                          GCancellable       *cancellable)
{
    GString *query;

    self->cancellable = cancellable;

    query = g_string_new ("SELECT ?url tracker:id(?urn) "
                          "WHERE { ?urn nie:url ?url ; nao:hasTag " STARRED_TAG "}");

    start_query_or_update (query,
                           on_get_starred_files_query_callback,
                           self,
                           TRUE,
                           cancellable);

    g_string_free (query, TRUE);
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
                     "DELETE { ?urn nao:hasTag " STARRED_TAG " }"
                     "WHERE { ?urn a nfo:FileDataObject ; nie:url ?url .");

    query = add_selection_filter (selection, query);

    g_string_append (query, "}\n");

    return query;
}

static GString *
nautilus_tag_manager_insert_tag (NautilusTagManager *self,
                                 GList              *selection,
                                 GString            *query)
{
    g_string_append (query,
                     "INSERT DATA { " STARRED_TAG " a nao:Tag .}\n"
                     "INSERT { ?urn nao:hasTag " STARRED_TAG " }"
                     "WHERE { ?urn a nfo:FileDataObject ; nie:url ?url .");

    query = add_selection_filter (selection, query);

    g_string_append (query, "}\n");

    return query;
}

gboolean
nautilus_tag_manager_file_is_starred (NautilusTagManager *self,
                                      const gchar        *file_name)
{
    return g_hash_table_contains (self->starred_files, file_name);
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
    GTask *task;

    task = user_data;

    on_query_callback (object,
                       result,
                       user_data,
                       on_get_file_ids_for_urls_cursor_callback,
                       GET_IDS_FOR_URLS,
                       g_task_get_cancellable (task));
}

static void
nautilus_tag_manager_get_file_ids_for_urls (GObject *object,
                                            GList   *selection,
                                            GTask   *task)
{
    GString *query;

    query = g_string_new ("SELECT ?url tracker:id(?urn) WHERE { ?urn nie:url ?url; .");

    query = add_selection_filter (selection, query);

    g_string_append (query, "}\n");

    start_query_or_update (query,
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

    start_query_or_update (query,
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

    task = g_task_new (self, cancellable, on_star_files_callback, NULL);
    g_task_set_task_data (task,
                          data,
                          NULL);

    nautilus_tag_manager_get_file_ids_for_urls (G_OBJECT (self), selection, task);
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

    start_query_or_update (query,
                           on_update_callback,
                           update_data,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

static void
on_tracker_notifier_events (TrackerNotifier *notifier,
                            GPtrArray       *events,
                            gpointer         user_data)
{
    TrackerNotifierEvent *event;
    NautilusTagManager *self;
    int i;
    const gchar *location_uri;
    const gchar *new_location_uri;
    GError *error = NULL;
    TrackerSparqlConnection *connection;
    TrackerSparqlCursor *cursor;
    GString *query;
    gboolean query_has_results;
    gint64 *id;
    GList *changed_files;
    NautilusFile *file;

    self = NAUTILUS_TAG_MANAGER (user_data);

    for (i = 0; i < events->len; i++)
    {
        event = g_ptr_array_index (events, i);

        location_uri = tracker_notifier_event_get_location (event);

        query = g_string_new ("");
        g_string_append_printf (query,
                                "SELECT ?url WHERE { ?urn nie:url ?url; nao:hasTag " STARRED_TAG " . FILTER (tracker:id(?urn) = %" G_GINT64_FORMAT ")}",
                                tracker_notifier_event_get_id (event));

        /* check if the file changed it's location and update hash table if so */
        new_location_uri = nautilus_tag_manager_file_with_id_changed_url (self->starred_files,
                                                                          tracker_notifier_event_get_id (event),
                                                                          location_uri);
        if (new_location_uri)
        {
            id = g_new0 (gint64, 1);
            *id = tracker_notifier_event_get_id (event);

            g_hash_table_remove (self->starred_files, new_location_uri);
            g_hash_table_insert (self->starred_files,
                                 g_strdup (location_uri),
                                 id);

            file = nautilus_file_get_by_uri (location_uri);
            changed_files = g_list_prepend (NULL, file);

            g_signal_emit_by_name (self, "starred-changed", changed_files);

            nautilus_file_list_free (changed_files);
        }

        connection = tracker_sparql_connection_get (NULL, &error);

        if (!connection)
        {
            g_printerr ("Couldn't obtain a direct connection to the Tracker store: %s",
                        error ? error->message : "unknown error");
            g_clear_error (&error);

            return;
        }

        cursor = tracker_sparql_connection_query (connection,
                                                  query->str,
                                                  NULL,
                                                  &error);

        if (error)
        {
            g_printerr ("Couldn't query the Tracker Store: '%s'", error->message);

            g_clear_error (&error);

            return;
        }

        if (cursor)
        {
            query_has_results = tracker_sparql_cursor_next (cursor, NULL, &error);

            /* if no results are found, then the file isn't marked as starred.
             * If needed, update the hashtable.
             */
            if (!query_has_results && location_uri && g_hash_table_contains (self->starred_files, location_uri))
            {
                g_hash_table_remove (self->starred_files, location_uri);

                file = nautilus_file_get_by_uri (location_uri);
                changed_files = g_list_prepend (NULL, file);

                g_signal_emit_by_name (self, "starred-changed", changed_files);

                nautilus_file_list_free (changed_files);
            }
            else if (query_has_results && location_uri && !g_hash_table_contains (self->starred_files, location_uri))
            {
                id = g_new0 (gint64, 1);
                *id = tracker_notifier_event_get_id (event);

                g_hash_table_insert (self->starred_files,
                                     g_strdup (location_uri),
                                     id);

                file = nautilus_file_get_by_uri (location_uri);
                changed_files = g_list_prepend (NULL, file);

                g_signal_emit_by_name (self, "starred-changed", changed_files);

                nautilus_file_list_free (changed_files);
            }

            g_object_unref (cursor);
        }

        g_object_unref (connection);

        g_string_free (query, TRUE);
    }
}

static void
nautilus_tag_manager_finalize (GObject *object)
{
    NautilusTagManager *self;

    self = NAUTILUS_TAG_MANAGER (object);

    g_signal_handlers_disconnect_by_func (self->notifier,
                                          G_CALLBACK (on_tracker_notifier_events),
                                          self);
    g_clear_object (&self->notifier);

    g_hash_table_destroy (self->starred_files);

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

void
nautilus_tag_manager_set_cancellable (NautilusTagManager *self,
                                      GCancellable       *cancellable)
{
    nautilus_tag_manager_query_starred_files (self, cancellable);

    self->notifier = tracker_notifier_new (NULL,
                                           TRACKER_NOTIFIER_FLAG_QUERY_LOCATION,
                                           cancellable,
                                           &self->notifier_error);
    if (self->notifier != NULL)
    {
        g_signal_connect (self->notifier,
                          "events",
                          G_CALLBACK (on_tracker_notifier_events),
                          self);
    }
}

static void
nautilus_tag_manager_init (NautilusTagManager *self)
{
    self->starred_files = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 (GDestroyNotify) g_free,
                                                 (GDestroyNotify) g_free);
}
