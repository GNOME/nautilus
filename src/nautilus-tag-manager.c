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
#include "nautilus-tracker-utilities.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_TAG_MANAGER
#include "nautilus-debug.h"

#include <tracker-sparql.h>

#include "config.h"

struct _NautilusTagManager
{
    GObject object;

    gboolean database_ok;
    TrackerSparqlConnection *db;
    TrackerNotifier *notifier;

    TrackerSparqlStatement *query_starred_files;
    TrackerSparqlStatement *query_file_is_starred;

    GHashTable *starred_file_uris;

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
    NautilusTagManager *tag_manager;
    GTask *task;
    GList *selection;
    gboolean star;
} UpdateData;

enum
{
    STARRED_CHANGED,
    LAST_SIGNAL
};

#define QUERY_STARRED_FILES \
    "SELECT ?file " \
    "{ " \
    "    ?file a nautilus:File ; " \
    "        nautilus:starred true . " \
    "}"

#define QUERY_FILE_IS_STARRED \
    "ASK " \
    "{ " \
    "    ~file a nautilus:File ; " \
    "        nautilus:starred true . " \
    "}"

static guint signals[LAST_SIGNAL];

static void
start_query_or_update (TrackerSparqlConnection *db,
                       GString                 *query,
                       GAsyncReadyCallback      callback,
                       gpointer                 user_data,
                       gboolean                 is_query,
                       GCancellable            *cancellable)
{
    g_autoptr (GError) error = NULL;

    if (!db)
    {
        g_message ("nautilus-tag-manager: No Tracker connection");
        return;
    }

    if (is_query)
    {
        tracker_sparql_connection_query_async (db,
                                               query->str,
                                               cancellable,
                                               callback,
                                               user_data);
    }
    else
    {
        tracker_sparql_connection_update_async (db,
                                                query->str,
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
    TrackerSparqlConnection *db;
    GError *error;
    UpdateData *data;

    data = user_data;

    error = NULL;

    db = TRACKER_SPARQL_CONNECTION (object);

    tracker_sparql_connection_update_finish (db, result, &error);

    if (error == NULL)
    {
        /* FIXME: make sure data->tag_manager->starred_file_uris is up to date */

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

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);

    g_hash_table_add (self->starred_file_uris, g_strdup (url));

    file = nautilus_file_get_by_uri (url);

    if (file)
    {
        changed_files = g_list_prepend (NULL, file);

        g_signal_emit_by_name (self, "starred-changed", changed_files);

        nautilus_file_list_free (changed_files);
    }
    else
    {
        DEBUG ("File %s is starred but not found", url);
    }

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
    if (!self->database_ok)
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

static GString *
nautilus_tag_manager_delete_tag (NautilusTagManager *self,
                                 GList              *selection)
{
    GString *query;
    NautilusFile *file;
    GList *l;

    query = g_string_new ("DELETE DATA {");

    for (l = selection; l != NULL; l = l->next)
    {
        g_autofree gchar *uri = NULL;

        file = l->data;

        uri = nautilus_file_get_uri (file);
        g_string_append_printf (query,
                                "    <%s> a nautilus:File ; "
                                "        nautilus:starred true . ",
                                uri);
    }

    g_string_append (query, "}");

    return query;
}

static GString *
nautilus_tag_manager_insert_tag (NautilusTagManager *self,
                                 GList              *selection)
{
    GString *query;
    NautilusFile *file;
    GList *l;

    query = g_string_new ("INSERT DATA {");

    for (l = selection; l != NULL; l = l->next)
    {
        g_autofree gchar *uri = NULL;

        file = l->data;

        uri = nautilus_file_get_uri (file);
        g_string_append_printf (query,
                                "    <%s> a nautilus:File ; "
                                "        nautilus:starred true . ",
                                uri);
    }

    g_string_append (query, "}");

    return query;
}

gboolean
nautilus_tag_manager_file_is_starred (NautilusTagManager *self,
                                      const gchar        *file_uri)
{
    return g_hash_table_contains (self->starred_file_uris, file_uri);
}

void
nautilus_tag_manager_star_files (NautilusTagManager  *self,
                                 GObject             *object,
                                 GList               *selection,
                                 GAsyncReadyCallback  callback,
                                 GCancellable        *cancellable)
{
    GString *query;
    g_autoptr (GError) error = NULL;
    GTask *task;
    UpdateData *update_data;

    DEBUG ("Starring %i files", g_list_length (selection));

    task = g_task_new (object, cancellable, callback, NULL);

    query = nautilus_tag_manager_insert_tag (self, selection);

    update_data = g_new0 (UpdateData, 1);
    update_data->task = task;
    update_data->tag_manager = self;
    update_data->selection = nautilus_file_list_copy (selection);
    update_data->star = TRUE;

    start_query_or_update (self->db,
                           query,
                           on_update_callback,
                           update_data,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
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

    query = nautilus_tag_manager_delete_tag (self, selection);

    update_data = g_new0 (UpdateData, 1);
    update_data->task = task;
    update_data->tag_manager = self;
    update_data->selection = nautilus_file_list_copy (selection);
    update_data->star = FALSE;

    start_query_or_update (self->db,
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
    GError *error = NULL;
    TrackerSparqlCursor *cursor;
    gboolean query_has_results = FALSE;
    gboolean starred;
    GList *changed_files;
    NautilusFile *changed_file;

    self = NAUTILUS_TAG_MANAGER (user_data);

    for (i = 0; i < events->len; i++)
    {
        event = g_ptr_array_index (events, i);

        file_url = tracker_notifier_event_get_urn (event);
        changed_file = NULL;

        DEBUG ("Got event for file %s", file_url);

        tracker_sparql_statement_bind_string (self->query_file_is_starred, "file", file_url);
        cursor = tracker_sparql_statement_execute (self->query_file_is_starred,
                                                   NULL,
                                                   &error);

        if (cursor)
        {
            query_has_results = tracker_sparql_cursor_next (cursor, NULL, &error);
        }

        if (error || !cursor || !query_has_results)
        {
            g_warning ("Couldn't query the starred files database: '%s'", error ? error->message : "(null error)");
            g_clear_error (&error);
            return;
        }

        starred = tracker_sparql_cursor_get_boolean (cursor, 0);
        if (starred)
        {
            gboolean inserted = g_hash_table_add (self->starred_file_uris, g_strdup (file_url));

            if (inserted)
            {
                DEBUG ("Added %s to starred files list", file_url);
                changed_file = nautilus_file_get_by_uri (file_url);
            }
        }
        else
        {
            gboolean removed = g_hash_table_remove (self->starred_file_uris, file_url);

            if (removed)
            {
                DEBUG ("Removed %s from starred files list", file_url);
                changed_file = nautilus_file_get_by_uri (file_url);
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
    g_clear_object (&self->db);
    g_clear_object (&self->query_file_is_starred);
    g_clear_object (&self->query_starred_files);

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
setup_database (NautilusTagManager  *self,
                GCancellable        *cancellable,
                GError             **error)
{
    const gchar *datadir;
    g_autofree gchar *store_path = NULL;
    g_autofree gchar *ontology_path = NULL;
    g_autoptr (GFile) store = NULL;
    g_autoptr (GFile) ontology = NULL;

    /* Open private database to store nautilus:starred property. */

    datadir = NAUTILUS_DATADIR;

    store_path = g_build_filename (g_get_user_data_dir (), "nautilus", "tags", NULL);
    ontology_path = g_build_filename (datadir, "ontology", NULL);

    store = g_file_new_for_path (store_path);
    ontology = g_file_new_for_path (ontology_path);

    self->db = tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                              store,
                                              ontology,
                                              cancellable,
                                              error);

    if (*error)
    {
        return FALSE;
    }

    /* Prepare reusable queries. */
    self->query_file_is_starred = tracker_sparql_connection_query_statement (self->db,
                                                                             QUERY_FILE_IS_STARRED,
                                                                             cancellable,
                                                                             error);

    if (*error)
    {
        return FALSE;
    }

    self->query_starred_files = tracker_sparql_connection_query_statement (self->db,
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

    self->database_ok = setup_database (self, cancellable, &error);

    if (error)
    {
        g_warning ("Unable to initialize tag manager: %s", error->message);
        return;
    }

    self->notifier = tracker_sparql_connection_create_notifier (self->db);

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
                                                     /* values are keys */
                                                     NULL);
}
