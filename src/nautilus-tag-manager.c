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
#include <tracker-sparql.h>

#define GINT64_TO_POINTER(i) ((gpointer) (gint64) (i))
#define GPOINTER_TO_GINT64(p) ((gint64) (p))

struct _NautilusTagManager
{
    GObject object;

    TrackerNotifier *notifier;
    GError *notifier_error;

    GQueue *all_tags;
    GCancellable *all_tags_cancellable;

    GHashTable *favorite_files;
    GCancellable *favorite_files_cancellable;
};

G_DEFINE_TYPE (NautilusTagManager, nautilus_tag_manager, G_TYPE_OBJECT);

static NautilusTagManager *tag_manager = NULL;

typedef enum
{
    GET_ALL_TAGS,
    GET_FAVORITE_FILES,
    GET_SELECTION_TAGS,
    GET_FILES_WITH_TAG,
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

enum
{
    FAVORITES_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static const gchar*
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

gchar*
parse_color_from_tag_id (const gchar *tag_id)
{
    gchar *color;

    if (g_strrstr (tag_id, "org:gnome:nautilus:tag"))
    {
        color = g_strdup (g_strrstr (tag_id, ":") + 1);
    }
    else
    {
        color = g_strdup ("rgb(220,220,220)");
    }

    return color;
}

gboolean
nautilus_tag_queue_has_tag (GQueue      *tag_queue,
                            const gchar *tag_name)
{
    GList *l;
    TagData *tag_data;

    for (l = g_queue_peek_head_link (tag_queue); l != NULL; l = l->next)
    {
        tag_data = l->data;

        if (g_strcmp0 (tag_name, tag_data->name) == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

GQueue*
nautilus_tag_copy_tag_queue (GQueue *queue)
{
    GQueue *result_queue;
    GList *l;
    TagData *tag_data, *new_tag_data;

    result_queue = g_queue_new ();

    for (l = g_queue_peek_head_link (queue); l != NULL; l = l->next)
    {
        tag_data = l->data;

        new_tag_data = g_new0 (TagData, 1);
        new_tag_data->id = g_strdup (tag_data->id);
        new_tag_data->name = g_strdup (tag_data->name);
        new_tag_data->url = g_strdup (tag_data->url);

        g_queue_push_tail (result_queue, new_tag_data);
    }

    return result_queue;
}

void
destroy_insert_task_data (gpointer data)
{
    InsertTaskData *task_data;

    task_data = data;

    nautilus_file_list_free (task_data->selection);
    g_hash_table_destroy (task_data->ids);
    g_free (data);
}

void
nautilus_tag_data_free (gpointer data)
{
    TagData *tag_data;

    tag_data = data;

    g_free (tag_data->id);
    g_free (tag_data->name);
    g_free (tag_data->url);
    g_free (tag_data);
}

static void
nautilus_tag_data_queue_free (gpointer data)
{
    GQueue *queue;

    queue = data;

    g_queue_free_full (queue, nautilus_tag_data_free);
}

static void
destroy_url_queue (gpointer data)
{
    GQueue *queue;

    queue = data;

    g_queue_free_full (queue, g_free);
}

TagData*
nautilus_tag_data_new (const gchar *id,
                       const gchar *name,
                       const gchar *url)
{
    TagData *data;

    data = g_new0 (TagData, 1);

    data->id = g_strdup (id);
    data->name = g_strdup (name);
    data->url = g_strdup (url);

    return data;
}

static GString*
add_selection_filter (GList   *selection,
                      GString *query)
{
    NautilusFile *file;
    GList *l;
    gchar *uri;

    g_string_append (query, " FILTER(?url IN (");

    for (l = selection; l != NULL; l = l->next)
    {
        file = l->data;

        uri = nautilus_file_get_uri (file);

        g_string_append_printf (query, "'%s'", uri);

        if (l->next != NULL)
        {
            g_string_append (query, ", ");
        }

        g_free (uri);
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
on_query_callback (GObject              *object,
                   GAsyncResult         *result,
                   gpointer              user_data,
                   GAsyncReadyCallback   callback,
                   OperationType         op_type,
                   GCancellable         *cancellable)
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
            if (op_type == GET_ALL_TAGS)
            {
                g_warning ("error while getting tags: %s\n", error->message);
            }
            else if (op_type == GET_SELECTION_TAGS)
            {
                g_task_return_pointer (task, g_task_get_task_data (task), nautilus_tag_data_queue_free);
            }
            else if (op_type == GET_FAVORITE_FILES)
            {
                g_warning ("Error on getting favorite files: %s", error->message);
            }
            else if (op_type == GET_IDS_FOR_URLS)
            {
                g_warning ("Error on getting id for url: %s", error->message);
                g_task_return_pointer (task, g_task_get_task_data (task), destroy_insert_task_data);
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
                    gpointer     user_data)
{
    TrackerSparqlConnection *connection;
    GError *error;
    GTask *task;

    task = user_data;

    error = NULL;

    connection = TRACKER_SPARQL_CONNECTION (object);

    tracker_sparql_connection_update_finish (connection, result, &error);

    if (error == NULL ||
        (error != NULL && error->code != G_IO_ERROR_CANCELLED))
    {
        g_task_return_boolean (task, TRUE);
    }
    else if (error && error->code == G_IO_ERROR_CANCELLED)
    {
       g_error_free (error);
    }
    else
    {
        g_task_return_error (task, error);
        g_warning ("error updating tags: %s", error->message);
    }
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
            if (op_type == GET_SELECTION_TAGS)
            {
                g_task_return_pointer (task, g_task_get_task_data (task), nautilus_tag_data_queue_free);

            }
            else if (op_type == GET_FILES_WITH_TAG)
            {
                g_task_return_pointer (task, g_task_get_task_data (task), destroy_url_queue);
            }
            else if (op_type == GET_IDS_FOR_URLS)
            {
                g_task_return_pointer (task, g_task_get_task_data (task), destroy_insert_task_data);
            }
        }
    }

    return success;
}

static void
on_get_all_tags_cursor_callback (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    const gchar *id;
    const gchar *name;
    gboolean success;
    NautilusTagManager *self;
    TagData *tag_data;

    cursor = TRACKER_SPARQL_CURSOR (object);

    self = NAUTILUS_TAG_MANAGER (user_data);

    success = get_query_status (cursor, result, GET_ALL_TAGS, NULL);
    if (!success)
    {
        return;
    }

    id = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    name = tracker_sparql_cursor_get_string (cursor, 1, NULL);

    tag_data = nautilus_tag_data_new (id, name, NULL);

    g_queue_push_tail (self->all_tags, tag_data);

    tracker_sparql_cursor_next_async (cursor,
                                      self->all_tags_cancellable,
                                      on_get_all_tags_cursor_callback,
                                      self);
}

static void
on_get_all_tags_query_callback (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
    NautilusTagManager *self;

    self = NAUTILUS_TAG_MANAGER (user_data);

    on_query_callback (object,
                       result,
                       user_data,
                       on_get_all_tags_cursor_callback,
                       GET_ALL_TAGS,
                       self->all_tags_cancellable);
}

GQueue*
nautilus_tag_manager_get_all_tags (NautilusTagManager *self)
{
    return self->all_tags;
}

GList*
nautilus_tag_manager_get_favorite_files (NautilusTagManager *self)
{
    return g_hash_table_get_keys (self->favorite_files);
}

static void
nautilus_tag_manager_query_all_tags (NautilusTagManager  *self,
                                     GCancellable *cancellable)
{
    GString *query;

    if (self->all_tags)
    {
        g_queue_free_full (self->all_tags, nautilus_tag_data_free);
    }
    self->all_tags = g_queue_new ();

    self->all_tags_cancellable = cancellable;

    query = g_string_new ("SELECT ?urn ?label WHERE { ?urn a nao:Tag ; nao:prefLabel ?label . } ORDER BY ?label");

    start_query_or_update (query,
                           on_get_all_tags_query_callback,
                           self,
                           TRUE,
                           cancellable);

    g_string_free (query, TRUE);
}

static void
on_get_favorite_files_cursor_callback (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    const gchar *url;
    gint64 id;
    gboolean success;
    NautilusTagManager *self;

    cursor = TRACKER_SPARQL_CURSOR (object);

    self = NAUTILUS_TAG_MANAGER (user_data);

    success = get_query_status (cursor, result, GET_FAVORITE_FILES, NULL);
    if (!success)
    {
        return;
    }

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    id = tracker_sparql_cursor_get_integer (cursor, 1);

    g_hash_table_insert (self->favorite_files,
                         g_strdup (url),
                         GINT64_TO_POINTER (id));
    g_signal_emit_by_name (self, "favorites-changed", NULL);

    tracker_sparql_cursor_next_async (cursor,
                                      self->favorite_files_cancellable,
                                      on_get_favorite_files_cursor_callback,
                                      self);
}

static void
on_get_favorite_files_query_callback (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
    NautilusTagManager *self;

    self = NAUTILUS_TAG_MANAGER (user_data);

    on_query_callback (object,
                       result,
                       user_data,
                       on_get_favorite_files_cursor_callback,
                       GET_FAVORITE_FILES,
                       self->favorite_files_cancellable);
}


static void
nautilus_tag_manager_query_favorite_files (NautilusTagManager *self,
                                           GCancellable       *cancellable)
{
    GString *query;

    self->favorite_files_cancellable = cancellable;

    query = g_string_new ("SELECT ?url tracker:id(?urn) WHERE { ?urn nie:url ?url ; nao:hasTag nao:predefined-tag-favorite}");

    start_query_or_update (query,
                           on_get_favorite_files_query_callback,
                           self,
                           TRUE,
                           cancellable);

    g_string_free (query, TRUE);
}

static void
on_get_selection_tags_cursor_callback (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    const gchar *id;
    const gchar *name;
    const gchar *url;
    gboolean success;
    TagData *tag_data;
    GTask *task;
    GQueue *selection_tags;

    cursor = TRACKER_SPARQL_CURSOR (object);

    task = user_data;

    selection_tags = g_task_get_task_data (task);

    success = get_query_status (cursor, result, GET_SELECTION_TAGS, user_data);
    if (!success)
    {
        return;
    }

    id = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    name = tracker_sparql_cursor_get_string (cursor, 1, NULL);
    url = tracker_sparql_cursor_get_string (cursor, 2, NULL);

    tag_data = nautilus_tag_data_new (id, name, url);

    g_queue_push_tail (selection_tags, tag_data);

    tracker_sparql_cursor_next_async (cursor,
                                      g_task_get_cancellable (task),
                                      on_get_selection_tags_cursor_callback,
                                      task);
}

static void
on_get_selection_tags_query_callback (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
    GTask *task;

    task = user_data;

    on_query_callback (object,
                       result,
                       task,
                       on_get_selection_tags_cursor_callback,
                       GET_SELECTION_TAGS,
                       g_task_get_cancellable (task));
}

gpointer
nautilus_tag_manager_gpointer_task_finish (GObject      *source_object,
                                           GAsyncResult *res,
                                           GError      **error)
{
    g_return_val_if_fail (g_task_is_valid (res, source_object), FALSE);

    return g_task_propagate_pointer (G_TASK (res), error);
}

void
nautilus_tag_manager_get_selection_tags (GObject            *object,
                                         GList              *selection,
                                         GAsyncReadyCallback callback,
                                         GCancellable       *cancellable)
{
    GString *query;
    GQueue *selection_tags;
    GTask *task;

    selection_tags = g_queue_new ();

    task = g_task_new (object, cancellable, callback, NULL);
    g_task_set_task_data (task,
                          selection_tags,
                          nautilus_tag_data_queue_free);

    query = g_string_new ("SELECT ?tag nao:prefLabel(?tag) ?url WHERE"
                          "{ ?urn a nfo:FileDataObject ; nao:hasTag ?tag ; nie:url ?url .");

    query = add_selection_filter (selection, query);

    g_string_append (query, "} ORDER BY (?tag)");

    start_query_or_update (query,
                           on_get_selection_tags_query_callback,
                           task,
                           TRUE,
                           cancellable);

    g_string_free (query, TRUE);
}

static void
on_get_files_with_tag_cursor_callback (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    cursor = TRACKER_SPARQL_CURSOR (object);
    gchar *url;
    gboolean success;
    GTask *task;
    GQueue *files_with_tag;

    task = user_data;
    files_with_tag = g_task_get_task_data (task);

    success = get_query_status (cursor, result, GET_FILES_WITH_TAG, task);
    if (!success)
    {
        return;
    }

    url = g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL));
    g_queue_push_tail (files_with_tag, url);

    tracker_sparql_cursor_next_async (cursor,
                                      g_task_get_cancellable (task),
                                      on_get_files_with_tag_cursor_callback,
                                      task);
}

static void
on_get_files_with_tag_query_callback (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
    GTask *task;

    task = user_data;

    on_query_callback (object,
                       result,
                       user_data,
                       on_get_files_with_tag_cursor_callback,
                       GET_FILES_WITH_TAG,
                       g_task_get_cancellable (task));
}

void
nautilus_tag_manager_get_files_with_tag (NautilusTagManager  *self,
                                         GObject             *object,
                                         const gchar         *tag_name,
                                         GAsyncReadyCallback  callback,
                                         GCancellable        *cancellable)
{
    GString *query;
    GQueue *files_with_tag;
    GError *error;
    GTask *task;

    files_with_tag = g_queue_new ();

    task = g_task_new (object, cancellable, callback, NULL);
    g_task_set_task_data (task,
                          files_with_tag,
                          nautilus_tag_data_queue_free);

    if (!nautilus_tag_queue_has_tag (self->all_tags, tag_name))
    {
        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_INVALID_ARGUMENT,
                             "this tag doesn't exist");

        g_task_return_error (task, error);

        return;
    }

    query = g_string_new ("SELECT ?url WHERE { ?urn a nfo:FileDataObject ;"
                          " nie:url ?url ; nao:hasTag ?tag . ");

    g_string_append_printf (query, "?tag nao:prefLabel '%s' }", tag_name);

    start_query_or_update (query,
                           on_get_files_with_tag_query_callback,
                           task,
                           TRUE,
                           cancellable);

    g_string_free (query, TRUE);
}

void
nautilus_tag_manager_remove_tag (NautilusTagManager  *self,
                                 GObject             *object,
                                 const gchar         *tag_name,
                                 GAsyncReadyCallback  callback,
                                 GCancellable        *cancellable)
{
    GString *query;
    GError *error;
    GList *l;
    TagData *tag_data;
    GTask *task;

    task = g_task_new (object, cancellable, callback, NULL);

    if (!nautilus_tag_queue_has_tag (self->all_tags, tag_name))
    {
        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_INVALID_ARGUMENT,
                             "this tag doesn't exist");

        g_task_return_error (task, error);

        return;
    }

    query = g_string_new ("");
    g_string_append_printf (query,
                            "DELETE { ?urn nao:hasTag ?label } "
                            "WHERE { ?urn nie:url ?f . "
                            "?label nao:prefLabel '%s' }\n",
                            tag_name);

    for (l = g_queue_peek_head_link (self->all_tags); l != NULL; l = l->next)
    {
        tag_data = l->data;

        if (g_strcmp0 (tag_name, tag_data->name) == 0)
        {
            g_string_append_printf (query,
                                    "DELETE { <%s> a rdfs:Resource }",
                                    tag_data->id);

            break;
        }
    }

    start_query_or_update (query,
                           on_update_callback,
                           task,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

static GString*
nautilus_tag_manager_delete_tag (NautilusTagManager  *self,
                                 GList               *selection,
                                 GString             *query,
                                 TagData             *tag_data,
                                 gboolean             favorite_tag)
{

    if (!favorite_tag)
    {
        if (!nautilus_tag_queue_has_tag (self->all_tags, tag_data->name))
        {
            return query;
        }

        g_string_append (query,
                         "DELETE { ?urn nao:hasTag ?label } "
                         "WHERE { ?urn a nfo:FileDataObject ; nie:url ?url . ");

        g_string_append_printf (query,
                                "?label nao:prefLabel '%s' . ",
                                tag_data->name);
    }
    else
    {
        g_string_append (query,
                         "DELETE { ?urn nao:hasTag nao:predefined-tag-favorite }"
                         "WHERE { ?urn a nfo:FileDataObject ; nie:url ?url .");
    }

    query = add_selection_filter (selection, query);

    g_string_append (query, "}\n");

    return query;
}

static GString*
nautilus_tag_manager_insert_tag (NautilusTagManager  *self,
                                 GList               *selection,
                                 GString             *query,
                                 TagData             *tag_data,
                                 gboolean             favorite_tag)
{
    g_autofree gchar *tag_color = NULL;

    if (!favorite_tag)
    {
        tag_color = parse_color_from_tag_id (tag_data->id);

        if (!nautilus_tag_queue_has_tag (self->all_tags, tag_data->name))
        {
            g_string_append_printf (query,
                                    "INSERT DATA { <org:gnome:nautilus:tag:%s:%s> a nao:Tag ; nao:prefLabel '%s' }\n",
                                    tag_data->name,
                                    tag_color,
                                    tag_data->name);
        }

        query = g_string_append (query,
                                 "INSERT { ?urn nao:hasTag ?label }"
                                 "WHERE { ?urn a nfo:FileDataObject ; nie:url ?url . ");

        g_string_append_printf (query, "?label nao:prefLabel '%s'", tag_data->name);
    }
    else
    {
        g_string_append (query,
                         "INSERT { ?urn nao:hasTag nao:predefined-tag-favorite }"
                         "WHERE { ?urn a nfo:FileDataObject ; nie:url ?url .");
    }

    query = add_selection_filter (selection, query);

    g_string_append (query, "}\n");

    return query;
}

gboolean
nautilus_tag_manager_update_tags_finish (GObject      *source_object,
                                         GAsyncResult *res,
                                         GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (res, source_object), FALSE);

    return g_task_propagate_boolean (G_TASK (res), error);
}

void
nautilus_tag_manager_update_tags (NautilusTagManager *self,
                                  GObject            *object,
                                  GList              *selection,
                                  GQueue             *selection_tags,
                                  GQueue             *new_selection_tags,
                                  GAsyncReadyCallback callback,
                                  GCancellable       *cancellable)
{
    GString *query;
    GList *l;
    TagData *tag_data;
    gchar *current_tag_name;
    GTask *task;

    task = g_task_new (object, cancellable, callback, NULL);

    query = g_string_new ("");

    for (l = g_queue_peek_head_link (new_selection_tags); l != NULL; l = l->next)
    {
        tag_data = l->data;

        if (!nautilus_tag_queue_has_tag (selection_tags, tag_data->name))
        {
            query = nautilus_tag_manager_insert_tag (self,
                                                     selection,
                                                     query,
                                                     tag_data,
                                                     FALSE);
        }
    }

    for (l = g_queue_peek_head_link (selection_tags); l != NULL; l = l->next)
    {
        tag_data = l->data;

        if (!nautilus_tag_queue_has_tag (new_selection_tags, tag_data->name) &&
            tag_data->name != NULL)
        {
            query = nautilus_tag_manager_delete_tag (self,
                                                     selection,
                                                     query,
                                                     tag_data,
                                                     FALSE);
        }

        current_tag_name = tag_data->name;
        while (TRUE)
        {
            if (l->next == NULL)
            {
                break;
            }

            tag_data = l->next->data;
            if (g_strcmp0 (tag_data->name, current_tag_name) == 0)
            {
                l = l->next;
            }
            else
            {
                break;
            }
        }
    }

    start_query_or_update (query,
                           on_update_callback,
                           task,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

gboolean
nautilus_tag_manager_file_is_favorite (NautilusTagManager *self,
                                       const gchar        *file_name)
{
    return g_hash_table_contains (self->favorite_files, file_name);
}

static void
on_get_file_ids_for_urls_cursor_callback (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    GTask *task;
    gint64 id;
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

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    id = tracker_sparql_cursor_get_integer (cursor, 1);

    for (l = data->selection; l != NULL; l = l->next)
    {
        file_url = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

        if (g_strcmp0 (file_url, url) == 0)
        {
            g_hash_table_insert (data->ids,
                                 g_strdup (url),
                                 GINT64_TO_POINTER (id));

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
on_star_file_callback (GObject      *object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    NautilusTagManager *self;
    GString *query;
    InsertTaskData *data;
    g_autoptr (GError) error = NULL;
    GTask *task;
    gchar* uri;
    gint64 id;
    GList *l;

    self = NAUTILUS_TAG_MANAGER (object);

    data = nautilus_tag_manager_gpointer_task_finish (object, res, &error);
    task = user_data;
    g_clear_object (&task);

    task = g_task_new (data->object, data->cancellable, data->callback, NULL);

    query = g_string_new ("");

    query = nautilus_tag_manager_insert_tag (self,
                                             data->selection,
                                             query,
                                             NULL,
                                             TRUE);

    for (l = data->selection; l != NULL; l = l->next)
    {
        uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

        if (g_hash_table_contains (data->ids, uri))
        {
            id = GPOINTER_TO_GINT64 (g_hash_table_lookup (data->ids, uri));
            g_hash_table_insert (self->favorite_files,
                                 nautilus_file_get_uri (NAUTILUS_FILE (l->data)),
                                 GINT64_TO_POINTER (id));
        }

        g_free (uri);
    }
    g_signal_emit_by_name (self, "favorites-changed", NULL);

    start_query_or_update (query,
                           on_update_callback,
                           task,
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
                                       NULL);
    data->callback = callback;
    data->object = object;
    data->cancellable = cancellable;

    task = g_task_new (self, cancellable, on_star_file_callback, NULL);
    g_task_set_task_data (task,
                          data,
                          destroy_insert_task_data);

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
    NautilusFile *file;
    gchar *uri = NULL;
    GList *l;

    file = NAUTILUS_FILE (selection->data);
    uri = nautilus_file_get_uri (file);

    task = g_task_new (object, cancellable, callback, NULL);

    query = g_string_new ("");

    query = nautilus_tag_manager_delete_tag (self,
                                             selection,
                                             query,
                                             NULL,
                                             TRUE);

    for (l = selection; l != NULL; l = l->next)
    {
        uri = nautilus_file_get_uri (NAUTILUS_FILE (l->data));

        g_hash_table_remove (self->favorite_files, uri);

        g_free (uri);
    }
    g_signal_emit_by_name (self, "favorites-changed", NULL);

    start_query_or_update (query,
                           on_update_callback,
                           task,
                           FALSE,
                           cancellable);

    g_string_free (query, TRUE);
}

void
on_tracker_notifier_events(TrackerNotifier *notifier,
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

    self = NAUTILUS_TAG_MANAGER (user_data);

    for (i = 0; i < events->len; i++)
    {
        event = g_ptr_array_index (events, i);

        location_uri = tracker_notifier_event_get_location (event);

        query = g_string_new ("");
        g_string_append_printf (query,
                                "SELECT ?url WHERE { ?urn nie:url ?url; nao:hasTag nao:predefined-tag-favorite . FILTER (tracker:id(?urn) = %ld)}",
                                tracker_notifier_event_get_id (event));

        /* check if the file changed it's location and update hash table if so */
        new_location_uri = nautilus_tag_manager_file_with_id_changed_url (self->favorite_files,
                                                                          tracker_notifier_event_get_id (event),
                                                                          location_uri);
        if (new_location_uri)
        {
            g_hash_table_remove (self->favorite_files, new_location_uri);
            g_hash_table_insert (self->favorite_files,
                                 g_strdup (location_uri),
                                 GINT64_TO_POINTER (tracker_notifier_event_get_id (event)));
            g_signal_emit_by_name (self, "favorites-changed", NULL);
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

            /* if no results are found, then the file isn't marked as favorite.
             * If needed, update the hashtable.
             */
            if (!query_has_results && location_uri && g_hash_table_contains (self->favorite_files, location_uri))
            {
                g_hash_table_remove (self->favorite_files, location_uri);
                g_signal_emit_by_name (self, "favorites-changed", NULL);
            }
            else if (query_has_results && location_uri && !g_hash_table_contains (self->favorite_files, location_uri))
            {
                g_hash_table_insert (self->favorite_files,
                                     g_strdup (location_uri),
                                     GINT64_TO_POINTER (tracker_notifier_event_get_id (event)));
                g_signal_emit_by_name (self, "favorites-changed", NULL);
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

    if (self->all_tags)
    {
        g_queue_free_full (self->all_tags, nautilus_tag_data_free);
    }

    g_signal_handlers_disconnect_by_func (self->notifier,
                                          G_CALLBACK (on_tracker_notifier_events),
                                          self);
    g_clear_object (&self->notifier);

    g_hash_table_destroy (self->favorite_files);

    G_OBJECT_CLASS (nautilus_tag_manager_parent_class)->finalize (object);
}

static void
nautilus_tag_manager_class_init (NautilusTagManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_tag_manager_finalize;

    signals[FAVORITES_CHANGED] = g_signal_new ("favorites-changed",
                                               NAUTILUS_TYPE_TAG_MANAGER,
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL,
                                               NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE,
                                               0);
}

NautilusTagManager* nautilus_tag_manager_new (GCancellable *cancellable_tags,
                                              GCancellable *cancellable_notifier,
                                              GCancellable *cancellable_favorite)
{
    //gchar *classes[] = { "nao:hasTag", NULL };

    if (tag_manager != NULL)
    {
        return g_object_ref (tag_manager);
    }

    tag_manager = g_object_new (NAUTILUS_TYPE_TAG_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (tag_manager), (gpointer)&tag_manager);

    nautilus_tag_manager_query_all_tags (tag_manager, cancellable_tags);

    nautilus_tag_manager_query_favorite_files (tag_manager, cancellable_favorite);

    tag_manager->notifier = tracker_notifier_new (NULL,
                                                  TRACKER_NOTIFIER_FLAG_QUERY_LOCATION,
                                                  cancellable_notifier,
                                                  &tag_manager->notifier_error);

    g_signal_connect (tag_manager->notifier,
                      "events",
                      G_CALLBACK (on_tracker_notifier_events),
                      tag_manager);

    return tag_manager;
}

static void
nautilus_tag_manager_init (NautilusTagManager *self)
{
    self->favorite_files = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  NULL);
}
