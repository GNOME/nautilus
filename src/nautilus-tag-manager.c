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
#define G_LOG_DOMAIN "nautilus-tag-manager"

#include "nautilus-tag-manager.h"
#include "nautilus-file.h"
#include "nautilus-file-undo-operations.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-tracker-utilities.h"

#include <gio/gunixinputstream.h>
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
    GFile *home;

    GList *pending_changed_files;

    GCancellable *cancellable;
};

G_DEFINE_TYPE (NautilusTagManager, nautilus_tag_manager, G_TYPE_OBJECT);

static NautilusTagManager *tag_manager = NULL;

/* See nautilus_tag_manager_new_dummy() documentation for details. */
static gboolean make_dummy_instance = FALSE;

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

/* Limit to 10MB output from Tracker -- surely, nobody has over a million starred files. */
#define TRACKER2_MAX_IMPORT_BYTES 10 * 1024 * 1024

static gchar *
tracker2_migration_stamp (void)
{
    return g_build_filename (g_get_user_data_dir (), "nautilus", "tracker2-migration-complete", NULL);
}

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
        if (!nautilus_file_undo_manager_is_operating ())
        {
            NautilusFileUndoInfo *undo_info;

            undo_info = nautilus_file_undo_info_starred_new (data->selection, data->star);
            nautilus_file_undo_manager_set_action (undo_info);

            g_object_unref (undo_info);
        }

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

/**
 * nautilus_tag_manager_get_starred_files:
 * @self: The tag manager singleton
 *
 * Returns: (element-type gchar*) (transfer full): A list of the starred NautilusFile.
 */
GList *
nautilus_tag_manager_get_starred_files (NautilusTagManager *self)
{
    GHashTableIter starred_iter;
    gchar *starred_uri;
    GList *starred_files = NULL;

    g_hash_table_iter_init (&starred_iter, self->starred_file_uris);
    while (g_hash_table_iter_next (&starred_iter, (gpointer *) &starred_uri, NULL))
    {
        g_autoptr (GFile) location = g_file_new_for_uri (starred_uri);
        NautilusFile *file = nautilus_file_get (location);

        /* Skip files outside $HOME, because we don't support starring these yet.
         * See comment on nautilus_tag_manager_can_star_contents() */
        if (g_file_has_prefix (location, self->home))
        {
            starred_files = g_list_prepend (starred_files, file);
        }
    }

    return starred_files;
}

static void
on_get_starred_files_cursor_callback (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    g_autoptr (GError) error = NULL;
    const gchar *url;
    gboolean success;
    NautilusTagManager *self;
    NautilusFile *file;

    cursor = TRACKER_SPARQL_CURSOR (object);

    self = NAUTILUS_TAG_MANAGER (user_data);

    success = tracker_sparql_cursor_next_finish (cursor, result, &error);

    if (!success)
    {
        if (error != NULL)
        {
            g_warning ("Error on getting all tags cursor callback: %s", error->message);
        }

        if (self->pending_changed_files != NULL)
        {
            g_signal_emit_by_name (self, "starred-changed", self->pending_changed_files);
            g_clear_pointer (&self->pending_changed_files, nautilus_file_list_free);
        }

        g_clear_object (&cursor);
        return;
    }

    url = tracker_sparql_cursor_get_string (cursor, 0, NULL);

    g_hash_table_add (self->starred_file_uris, g_strdup (url));

    file = nautilus_file_get_by_uri (url);

    if (file)
    {
        self->pending_changed_files = g_list_prepend (self->pending_changed_files, file);
    }
    else
    {
        g_debug ("File %s is starred but not found", url);
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

    g_debug ("Starring %i files", g_list_length (selection));

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

    g_debug ("Unstarring %i files", g_list_length (selection));

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
    const gchar *file_url;
    GError *error = NULL;
    TrackerSparqlCursor *cursor;
    gboolean query_has_results = FALSE;
    gboolean starred;
    GList *changed_files;
    NautilusFile *changed_file;

    self = NAUTILUS_TAG_MANAGER (user_data);

    for (guint i = 0; i < events->len; i++)
    {
        event = g_ptr_array_index (events, i);

        file_url = tracker_notifier_event_get_urn (event);
        changed_file = NULL;

        g_debug ("Got event for file %s", file_url);

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
                g_debug ("Added %s to starred files list", file_url);
                changed_file = nautilus_file_get_by_uri (file_url);
            }
        }
        else
        {
            gboolean removed = g_hash_table_remove (self->starred_file_uris, file_url);

            if (removed)
            {
                g_debug ("Removed %s from starred files list", file_url);
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

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->notifier);
    g_clear_object (&self->db);
    g_clear_object (&self->query_file_is_starred);
    g_clear_object (&self->query_starred_files);
    g_clear_pointer (&self->pending_changed_files, nautilus_file_list_free);

    g_hash_table_destroy (self->starred_file_uris);
    g_clear_object (&self->home);

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

/**
 * nautilus_tag_manager_new:
 *
 * Returns: (transfer full): the #NautilusTagManager singleton object.
 */
NautilusTagManager *
nautilus_tag_manager_new (void)
{
    if (tag_manager != NULL)
    {
        return g_object_ref (tag_manager);
    }

    tag_manager = g_object_new (NAUTILUS_TYPE_TAG_MANAGER, NULL);
    g_object_add_weak_pointer (G_OBJECT (tag_manager), (gpointer) & tag_manager);

    return tag_manager;
}

/**
 * nautilus_tag_manager_new_dummy:
 *
 * Creates a dummy tag manager without database.
 *
 * Useful only for tests where the tag manager is needed but not being tested
 * and we don't want to fail the tests due to irrelevant D-Bus failures.
 *
 * Returns: (transfer full): the #NautilusTagManager singleton object.
 */
NautilusTagManager *
nautilus_tag_manager_new_dummy (void)
{
    make_dummy_instance = TRUE;
    return nautilus_tag_manager_new ();
}

/**
 * nautilus_tag_manager_get:
 *
 * Returns: (transfer none): the #NautilusTagManager singleton object.
 */
NautilusTagManager *
nautilus_tag_manager_get (void)
{
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

static void
nautilus_tag_manager_init (NautilusTagManager *self)
{
    g_autoptr (GError) error = NULL;

    self->starred_file_uris = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     (GDestroyNotify) g_free,
                                                     /* values are keys */
                                                     NULL);
    self->home = g_file_new_for_path (g_get_home_dir ());

    if (make_dummy_instance)
    {
        /* Skip database initiation for nautilus_tag_manager_new_dummy(). */
        return;
    }

    self->cancellable = g_cancellable_new ();
    self->database_ok = setup_database (self, self->cancellable, &error);
    if (error)
    {
        g_warning ("Unable to initialize tag manager: %s", error->message);
        return;
    }

    self->notifier = tracker_sparql_connection_create_notifier (self->db);

    nautilus_tag_manager_query_starred_files (self, self->cancellable);

    g_signal_connect (self->notifier,
                      "events",
                      G_CALLBACK (on_tracker_notifier_events),
                      self);
}

gboolean
nautilus_tag_manager_can_star_contents (NautilusTagManager *self,
                                        GFile              *directory)
{
    /* We only allow files to be starred inside the home directory for now.
     * This avoids the starred files database growing too big.
     * See https://gitlab.gnome.org/GNOME/nautilus/-/merge_requests/553#note_903108
     */
    return g_file_has_prefix (directory, self->home) || g_file_equal (directory, self->home);
}

gboolean
nautilus_tag_manager_can_star_location (NautilusTagManager *self,
                                        GFile              *directory)
{
    /* We only allow files to be starred inside the home directory for now.
     * This avoids the starred files database growing too big.
     * See https://gitlab.gnome.org/GNOME/nautilus/-/merge_requests/553#note_903108
     */
    return g_file_has_prefix (directory, self->home);
}

static void
update_moved_uris_callback (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
    g_autoptr (GError) error = NULL;

    tracker_sparql_connection_update_finish (TRACKER_SPARQL_CONNECTION (object),
                                             result,
                                             &error);

    if (error != NULL && error->code != G_IO_ERROR_CANCELLED)
    {
        g_warning ("Error updating moved uris: %s", error->message);
    }
}

/**
 * nautilus_tag_manager_update_moved_uris:
 * @self: The tag manager singleton
 * @src: The original location as a #GFile
 * @dest: The new location as a #GFile
 *
 * Checks whether the rename/move operation (@src to @dest) has modified
 * the URIs of any starred files, and updates the database accordingly.
 */
void
nautilus_tag_manager_update_moved_uris (NautilusTagManager *self,
                                        GFile              *src,
                                        GFile              *dest)
{
    GHashTableIter starred_iter;
    gchar *starred_uri;
    g_autoptr (GPtrArray) old_uris = NULL;
    g_autoptr (GPtrArray) new_uris = NULL;
    g_autoptr (GString) query = NULL;

    if (!self->database_ok)
    {
        g_message ("nautilus-tag-manager: No Tracker connection");
        return;
    }

    old_uris = g_ptr_array_new ();
    new_uris = g_ptr_array_new_with_free_func (g_free);

    g_hash_table_iter_init (&starred_iter, self->starred_file_uris);
    while (g_hash_table_iter_next (&starred_iter, (gpointer *) &starred_uri, NULL))
    {
        g_autoptr (GFile) starred_location = NULL;
        g_autofree gchar *relative_path = NULL;

        starred_location = g_file_new_for_uri (starred_uri);

        if (g_file_equal (starred_location, src))
        {
            /* The moved file/folder is starred */
            g_ptr_array_add (old_uris, starred_uri);
            g_ptr_array_add (new_uris, g_file_get_uri (dest));
            continue;
        }

        relative_path = g_file_get_relative_path (src, starred_location);
        if (relative_path != NULL)
        {
            /* The starred file/folder is descendant of the moved/renamed directory */
            g_autoptr (GFile) new_location = NULL;

            new_location = g_file_resolve_relative_path (dest, relative_path);

            g_ptr_array_add (old_uris, starred_uri);
            g_ptr_array_add (new_uris, g_file_get_uri (new_location));
        }
    }

    if (new_uris->len == 0)
    {
        /* No starred files are affected by this move/rename */
        return;
    }

    g_debug ("Updating moved URI for %i starred files", new_uris->len);

    query = g_string_new ("DELETE DATA {");

    for (guint i = 0; i < old_uris->len; i++)
    {
        gchar *old_uri = g_ptr_array_index (old_uris, i);
        g_string_append_printf (query,
                                "    <%s> a nautilus:File ; "
                                "        nautilus:starred true . ",
                                old_uri);
    }

    g_string_append (query, "} ; INSERT DATA {");

    for (guint i = 0; i < new_uris->len; i++)
    {
        gchar *new_uri = g_ptr_array_index (new_uris, i);
        g_string_append_printf (query,
                                "    <%s> a nautilus:File ; "
                                "        nautilus:starred true . ",
                                new_uri);
    }

    g_string_append (query, "}");

    tracker_sparql_connection_update_async (self->db,
                                            query->str,
                                            self->cancellable,
                                            update_moved_uris_callback,
                                            NULL);
}

static void
process_tracker2_data_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
    NautilusTagManager *self = NAUTILUS_TAG_MANAGER (source_object);
    g_autofree gchar *path = tracker2_migration_stamp ();
    g_autoptr (GError) error = NULL;

    tracker_sparql_connection_update_finish (self->db, res, &error);

    if (!error)
    {
        g_debug ("Data migration was successful. Creating stamp %s", path);

        g_file_set_contents (path, "", -1, &error);
        if (error)
        {
            g_warning ("Failed to create %s after migration: %s", path, error->message);
        }
    }
    else
    {
        g_warning ("Error during data migration: %s", error->message);
    }
}

static void
process_tracker2_data (NautilusTagManager *self,
                       GBytes             *key_file_data)
{
    g_autoptr (GKeyFile) key_file = NULL;
    g_autoptr (GError) error = NULL;
    gchar **groups, **group;
    GList *selection = NULL;
    NautilusFile *file;

    key_file = g_key_file_new ();
    g_key_file_load_from_bytes (key_file,
                                key_file_data,
                                G_KEY_FILE_NONE,
                                &error);
    g_bytes_unref (key_file_data);

    if (error)
    {
        g_warning ("Tracker 2 migration: Failed to parse key file data: %s", error->message);
        return;
    }

    groups = g_key_file_get_groups (key_file, NULL);

    for (group = groups; *group != NULL; group++)
    {
        file = nautilus_file_get_by_uri (*group);

        if (file)
        {
            g_debug ("Tracker 2 migration: starring %s", *group);
            selection = g_list_prepend (selection, file);
        }
        else
        {
            g_debug ("Tracker 2 migration: couldn't get NautilusFile for %s", *group);
        }
    }

    nautilus_tag_manager_star_files (self,
                                     G_OBJECT (self),
                                     selection,
                                     process_tracker2_data_cb,
                                     self->cancellable);

    g_free (groups);
}

static void
export_tracker2_data_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
    GInputStream *stream = G_INPUT_STREAM (source_object);
    NautilusTagManager *self = NAUTILUS_TAG_MANAGER (user_data);
    g_autoptr (GError) error = NULL;
    GBytes *key_file_data;

    key_file_data = g_input_stream_read_bytes_finish (stream, res, &error);

    if (key_file_data)
    {
        process_tracker2_data (self, key_file_data);
    }
    else
    {
        g_warning ("Tracker2 migration: Failed to read data from pipe: %s", error->message);
    }
}

static void
child_watch_cb (GPid     pid,
                gint     status,
                gpointer user_data)
{
    g_debug ("Child %" G_PID_FORMAT " exited %s", pid,
             g_spawn_check_wait_status (status, NULL) ? "normally" : "abnormally");
    g_spawn_close_pid (pid);
}

static void
export_tracker2_data (NautilusTagManager *self)
{
    gchar *argv[] = {"tinysparql3", "export", "--2to3", "files-starred", "--keyfile", NULL};
    gint stdout_fd;
    GPid child_pid;
    g_autoptr (GError) error = NULL;
    gboolean success;
    g_autoptr (GInputStream) stream = NULL;
    GSpawnFlags flags;

    flags = G_SPAWN_DO_NOT_REAP_CHILD |
            G_SPAWN_STDERR_TO_DEV_NULL |
            G_SPAWN_SEARCH_PATH;
    success = g_spawn_async_with_pipes (NULL,
                                        argv,
                                        NULL,
                                        flags,
                                        NULL,
                                        NULL,
                                        &child_pid,
                                        NULL,
                                        &stdout_fd,
                                        NULL,
                                        &error);
    if (!success)
    {
        g_warning ("Tracker 2 migration: Couldn't run `tinysparql3`: %s", error->message);
        return;
    }

    g_child_watch_add (child_pid, child_watch_cb, NULL);

    stream = g_unix_input_stream_new (stdout_fd, TRUE);
    g_input_stream_read_bytes_async (stream,
                                     TRACKER2_MAX_IMPORT_BYTES,
                                     G_PRIORITY_LOW,
                                     self->cancellable,
                                     export_tracker2_data_cb,
                                     self);
}

void
nautilus_tag_manager_maybe_migrate_tracker2_data (NautilusTagManager *self)
{
    g_autofree gchar *path = tracker2_migration_stamp ();

    if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
        g_debug ("Tracker 2 migration: already completed.");
    }
    else
    {
        g_debug ("Tracker 2 migration: starting.");
        export_tracker2_data (self);
    }
}
