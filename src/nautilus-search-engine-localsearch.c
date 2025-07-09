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
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 */
#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "nautilus-search-engine-localsearch.h"

#include "nautilus-file.h"
#include "nautilus-query.h"
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-localsearch-utilities.h"

#include <string.h>
#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>

typedef enum
{
    SEARCH_FEATURE_NONE = 0,
    SEARCH_FEATURE_TERMS = 1 << 0,
    SEARCH_FEATURE_CONTENT = 1 << 1,
    SEARCH_FEATURE_MIMETYPE = 1 << 2,
    SEARCH_FEATURE_RECURSIVE = 1 << 3,
    SEARCH_FEATURE_ATIME = 1 << 4,
    SEARCH_FEATURE_MTIME = 1 << 5,
    SEARCH_FEATURE_CTIME = 1 << 6,
    SEARCH_FEATURE_LOCATION = 1 << 7,
} SearchFeatures;

struct _NautilusSearchEngineLocalsearch
{
    GObject parent_instance;

    TrackerSparqlConnection *connection;
    NautilusQuery *query;
    GHashTable *statements;

    gboolean query_pending;
    GQueue *hits_pending;

    gboolean fts_enabled;

    GCancellable *cancellable;
};

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineLocalsearch,
                         nautilus_search_engine_localsearch,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

static void
finalize (GObject *object)
{
    NautilusSearchEngineLocalsearch *self = NAUTILUS_SEARCH_ENGINE_LOCALSEARCH (object);

    if (self->cancellable)
    {
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
    }

    g_clear_object (&self->query);
    g_queue_free_full (self->hits_pending, g_object_unref);
    g_clear_pointer (&self->statements, g_hash_table_unref);
    /* This is a singleton, no need to unref. */
    self->connection = NULL;

    G_OBJECT_CLASS (nautilus_search_engine_localsearch_parent_class)->finalize (object);
}

#define BATCH_SIZE 100

static void
check_pending_hits (NautilusSearchEngineLocalsearch *self,
                    gboolean                         force_send)
{
    if (!force_send &&
        g_queue_get_length (self->hits_pending) < BATCH_SIZE)
    {
        return;
    }

    NautilusSearchHit *hit;
    g_autoptr (GPtrArray) hits = g_ptr_array_new_with_free_func (g_object_unref);

    g_debug ("Localsearch engine add hits");

    while ((hit = g_queue_pop_head (self->hits_pending)))
    {
        g_ptr_array_add (hits, hit);
    }

    if (hits->len > 0)
    {
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (self),
                                             g_steal_pointer (&hits));
    }
}

static void
search_finished (NautilusSearchEngineLocalsearch *self,
                 GError                          *error)
{
    g_debug ("Tracker engine finished");

    if (error == NULL)
    {
        check_pending_hits (self, TRUE);
    }
    else
    {
        g_queue_foreach (self->hits_pending, (GFunc) g_object_unref, NULL);
        g_queue_clear (self->hits_pending);
    }

    self->query_pending = FALSE;

    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
        g_debug ("Tracker engine error %s", error->message);
        nautilus_search_provider_error (NAUTILUS_SEARCH_PROVIDER (self), error->message);
    }
    else
    {
        nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (self),
                                           NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_debug ("Tracker engine finished and cancelled");
        }
        else
        {
            g_debug ("Tracker engine finished correctly");
        }
    }

    g_object_unref (self);
}

static void cursor_callback (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data);

static void
cursor_next (NautilusSearchEngineLocalsearch *self,
             TrackerSparqlCursor             *cursor)
{
    tracker_sparql_cursor_next_async (cursor,
                                      self->cancellable,
                                      cursor_callback,
                                      self);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    NautilusSearchEngineLocalsearch *self = NAUTILUS_SEARCH_ENGINE_LOCALSEARCH (user_data);
    GError *error = NULL;
    TrackerSparqlCursor *cursor;
    NautilusSearchHit *hit;
    const char *uri;
    const char *mtime_str;
    const char *atime_str;
    const char *ctime_str;
    const gchar *snippet;
    g_autoptr (GTimeZone) tz = NULL;
    gdouble rank, match;
    gboolean success;
    gchar *basename;

    cursor = TRACKER_SPARQL_CURSOR (object);
    success = tracker_sparql_cursor_next_finish (cursor, result, &error);

    if (!success)
    {
        search_finished (self, error);

        g_clear_error (&error);
        tracker_sparql_cursor_close (cursor);
        g_clear_object (&cursor);

        return;
    }

    /* We iterate result by result, not n at a time. */
    uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    rank = tracker_sparql_cursor_get_double (cursor, 1);
    mtime_str = tracker_sparql_cursor_get_string (cursor, 2, NULL);
    ctime_str = tracker_sparql_cursor_get_string (cursor, 3, NULL);
    atime_str = tracker_sparql_cursor_get_string (cursor, 4, NULL);
    basename = g_path_get_basename (uri);

    hit = nautilus_search_hit_new (uri);
    match = nautilus_query_matches_string (self->query, basename);
    nautilus_search_hit_set_fts_rank (hit, rank + match);
    g_free (basename);

    if (self->fts_enabled)
    {
        snippet = tracker_sparql_cursor_get_string (cursor, 5, NULL);
        if (snippet != NULL)
        {
            g_autofree gchar *escaped = NULL;
            g_autoptr (GString) buffer = NULL;
            /* Escape for markup, before adding our own markup. */
            escaped = g_markup_escape_text (snippet, -1);
            buffer = g_string_new (escaped);
            g_string_replace (buffer, "_NAUTILUS_SNIPPET_DELIM_START_", "<b>", 0);
            g_string_replace (buffer, "_NAUTILUS_SNIPPET_DELIM_END_", "</b>", 0);

            nautilus_search_hit_set_fts_snippet (hit, buffer->str);
        }
    }

    if (mtime_str != NULL ||
        atime_str != NULL ||
        ctime_str != NULL)
    {
        tz = g_time_zone_new_local ();
    }

    if (mtime_str != NULL)
    {
        g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 (mtime_str, tz);

        if (date == NULL)
        {
            g_warning ("unable to parse mtime: %s", mtime_str);
        }
        nautilus_search_hit_set_modification_time (hit, date);
    }

    if (atime_str != NULL)
    {
        g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 (atime_str, tz);

        if (date == NULL)
        {
            g_warning ("unable to parse atime: %s", atime_str);
        }
        nautilus_search_hit_set_access_time (hit, date);
    }

    if (ctime_str != NULL)
    {
        g_autoptr (GDateTime) date = g_date_time_new_from_iso8601 (ctime_str, tz);

        if (date == NULL)
        {
            g_warning ("unable to parse ctime: %s", ctime_str);
        }
        nautilus_search_hit_set_creation_time (hit, date);
    }

    g_queue_push_head (self->hits_pending, hit);
    check_pending_hits (self, FALSE);

    /* Get next */
    cursor_next (self, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
    NautilusSearchEngineLocalsearch *self = NAUTILUS_SEARCH_ENGINE_LOCALSEARCH (user_data);
    TrackerSparqlStatement *stmt;
    TrackerSparqlCursor *cursor;
    GError *error = NULL;

    stmt = TRACKER_SPARQL_STATEMENT (object);
    cursor = tracker_sparql_statement_execute_finish (stmt,
                                                      result,
                                                      &error);

    if (error != NULL)
    {
        search_finished (self, error);
        g_error_free (error);
    }
    else
    {
        cursor_next (self, cursor);
    }
}

static TrackerSparqlStatement *
create_statement (NautilusSearchProvider *provider,
                  SearchFeatures          features)
{
    NautilusSearchEngineLocalsearch *self = NAUTILUS_SEARCH_ENGINE_LOCALSEARCH (provider);
    GString *sparql;
    TrackerSparqlStatement *stmt;

#define VARIABLES \
        " ?url" \
        " ?rank"  \
        " ?mtime" \
        " ?ctime" \
        " ?atime" \
        " ?snippet"

#define TRIPLE_PATTERN \
        "?file a nfo:FileDataObject;" \
        "  nfo:fileLastModified ?mtime;" \
        "  nfo:fileLastAccessed ?atime;" \
        "  nfo:fileCreated ?ctime;" \
        "  nie:url ?url."

    sparql = g_string_new ("SELECT DISTINCT " VARIABLES " WHERE {");

    if (features & SEARCH_FEATURE_MIMETYPE)
    {
        g_string_append (sparql,
                         "  GRAPH ?g {"
                         "    ?content nie:isStoredAs ?file;"
                         "      nie:mimeType ?mime"
                         "  }");
    }

    if (features & SEARCH_FEATURE_TERMS)
    {
        if (features & SEARCH_FEATURE_CONTENT)
        {
            g_string_append (sparql,
                             " { "
                             "   SELECT ?file " VARIABLES " {"
                             "     GRAPH tracker:Documents {"
                             "       ?file a nfo:FileDataObject ."
                             "       ?content nie:isStoredAs ?file ."
                             "       ?content fts:match ~match ."
                             "       BIND(fts:rank(?content) AS ?rank) ."
                             "       BIND(fts:snippet(?content,"
                             "                        '_NAUTILUS_SNIPPET_DELIM_START_',"
                             "                        '_NAUTILUS_SNIPPET_DELIM_END_',"
                             "                        '…',"
                             "                        20) AS ?snippet)"
                             "     }"
                             "     GRAPH tracker:FileSystem {"
                             TRIPLE_PATTERN
                             "     }"
                             "   }"
                             "   ORDER BY DESC (?rank)"
                             " } UNION");
        }

        /* Note: Do not be fooled by `fts:match` below. It matches only the
         * filename here, unlike its usage above. This is because it's used
         * with `nfo:FileDataObject`, not `nie:InformationElement`. The only
         * full-text indexed property of `nfo:FileDataObject` is `nfo:fileName`.
         */
        g_string_append (sparql,
                         " {"
                         "   SELECT ?file " VARIABLES " {"
                         "     GRAPH tracker:FileSystem {"
                         TRIPLE_PATTERN
                         "       ?file fts:match ~match ."
                         "       BIND(fts:rank(?file) AS ?rank) ."
                         "     }"
                         "   }"
                         "   ORDER BY DESC (?rank)"
                         " }");
    }
    else
    {
        g_string_append (sparql,
                         " GRAPH tracker:FileSystem {"
                         TRIPLE_PATTERN
                         "   BIND (0 AS ?rank)"
                         " }");
    }

    g_string_append (sparql, " . FILTER( ");

    if (!(features & SEARCH_FEATURE_LOCATION))
    {
        /* Global search. Match any location. */
        g_string_append (sparql, "true");
    }
    else if (!(features & SEARCH_FEATURE_RECURSIVE))
    {
        g_string_append (sparql, "tracker:uri-is-parent(~location, ?url)");
    }
    else
    {
        /* STRSTARTS is faster than tracker:uri-is-descendant().
         * See https://gitlab.gnome.org/GNOME/tracker/-/issues/243
         */
        g_string_append (sparql, "STRSTARTS(?url, CONCAT (~location, '/'))");
    }

    if (features &
        (SEARCH_FEATURE_ATIME | SEARCH_FEATURE_MTIME | SEARCH_FEATURE_CTIME))
    {
        g_string_append (sparql, " && ");

        if (features & SEARCH_FEATURE_ATIME)
        {
            g_string_append (sparql, "?atime >= ~startTime^^xsd:dateTime");
            g_string_append (sparql, " && ?atime <= ~endTime^^xsd:dateTime");
        }
        else if (features & SEARCH_FEATURE_MTIME)
        {
            g_string_append (sparql, "?mtime >= ~startTime^^xsd:dateTime");
            g_string_append (sparql, " && ?mtime <= ~endTime^^xsd:dateTime");
        }
        else if (features & SEARCH_FEATURE_CTIME)
        {
            g_string_append (sparql, "?ctime >= ~startTime^^xsd:dateTime");
            g_string_append (sparql, " && ?ctime <= ~endTime^^xsd:dateTime");
        }
    }

    if (features & SEARCH_FEATURE_MIMETYPE)
    {
        g_string_append (sparql, " && CONTAINS(~mimeTypes, ?mime)");
    }

    g_string_append (sparql, ")}");

    stmt = tracker_sparql_connection_query_statement (self->connection,
                                                      sparql->str,
                                                      NULL,
                                                      NULL);
    g_string_free (sparql, TRUE);

    return stmt;
}

static gboolean
search_engine_localsearch_start (NautilusSearchProvider *provider,
                                 NautilusQuery          *query)
{
    NautilusSearchEngineLocalsearch *self = NAUTILUS_SEARCH_ENGINE_LOCALSEARCH (provider);
    g_autofree gchar *query_text = NULL;
    g_autoptr (GPtrArray) mimetypes = NULL;
    g_autoptr (GPtrArray) date_range = NULL;
    NautilusSearchTimeType type;
    TrackerSparqlStatement *stmt;
    SearchFeatures features = 0;

    g_set_object (&self->query, query);

    if (self->query_pending)
    {
        return FALSE;
    }

    if (self->connection == NULL)
    {
        g_warning ("Localsearch search engine has no connection");
        return FALSE;
    }

    g_debug ("Tracker engine start");
    g_object_ref (self);
    self->query_pending = TRUE;

    g_autoptr (GFile) location = nautilus_query_get_location (self->query);

    self->fts_enabled = nautilus_query_get_search_content (self->query);

    query_text = nautilus_query_get_text (self->query);
    mimetypes = nautilus_query_get_mime_types (self->query);
    date_range = nautilus_query_get_date_range (self->query);
    type = nautilus_query_get_search_type (self->query);

    if (query_text != NULL)
    {
        features |= SEARCH_FEATURE_TERMS;
    }
    if (self->fts_enabled)
    {
        features |= SEARCH_FEATURE_CONTENT;
    }
    if (nautilus_query_recursive (self->query))
    {
        features |= SEARCH_FEATURE_RECURSIVE;
    }
    if (mimetypes->len > 0)
    {
        features |= SEARCH_FEATURE_MIMETYPE;
    }

    if (date_range)
    {
        if (type == NAUTILUS_SEARCH_TIME_TYPE_LAST_ACCESS)
        {
            features |= SEARCH_FEATURE_ATIME;
        }
        else if (type == NAUTILUS_SEARCH_TIME_TYPE_LAST_MODIFIED)
        {
            features |= SEARCH_FEATURE_MTIME;
        }
        else if (type == NAUTILUS_SEARCH_TIME_TYPE_CREATED)
        {
            features |= SEARCH_FEATURE_CTIME;
        }
    }

    if (location != NULL)
    {
        features |= SEARCH_FEATURE_LOCATION;
    }

    stmt = g_hash_table_lookup (self->statements, GUINT_TO_POINTER (features));

    if (!stmt)
    {
        stmt = create_statement (provider, features);
        g_hash_table_insert (self->statements,
                             GUINT_TO_POINTER (features), stmt);
    }

    if (location != NULL)
    {
        g_autofree gchar *location_uri = g_file_get_uri (location);
        tracker_sparql_statement_bind_string (stmt, "location", location_uri);
    }

    if (query_text != NULL)
    {
        tracker_sparql_statement_bind_string (stmt, "match", query_text);
    }

    if (mimetypes->len > 0)
    {
        g_autoptr (GString) mimetype_str = NULL;

        for (guint i = 0; i < mimetypes->len; i++)
        {
            const gchar *mimetype;

            mimetype = g_ptr_array_index (mimetypes, i);

            if (!mimetype_str)
            {
                mimetype_str = g_string_new (mimetype);
            }
            else
            {
                g_string_append_printf (mimetype_str, ",%s", mimetype);
            }
        }

        tracker_sparql_statement_bind_string (stmt, "mimeTypes", mimetype_str->str);
    }

    if (date_range)
    {
        g_autofree gchar *initial_date_format = NULL;
        g_autofree gchar *end_date_format = NULL;
        GDateTime *initial_date;
        GDateTime *end_date;
        g_autoptr (GDateTime) shifted_end_date = NULL;

        initial_date = g_ptr_array_index (date_range, 0);
        end_date = g_ptr_array_index (date_range, 1);
        /* As we do for other searches, we want to make the end date inclusive.
         * For that, add a day to it */
        shifted_end_date = g_date_time_add_days (end_date, 1);

        initial_date_format = g_date_time_format_iso8601 (initial_date);
        end_date_format = g_date_time_format_iso8601 (shifted_end_date);

        tracker_sparql_statement_bind_string (stmt, "startTime",
                                              initial_date_format);
        tracker_sparql_statement_bind_string (stmt, "endTime",
                                              end_date_format);
    }

    self->cancellable = g_cancellable_new ();
    tracker_sparql_statement_execute_async (stmt,
                                            self->cancellable,
                                            query_callback,
                                            self);

    return TRUE;
}

static void
nautilus_search_engine_localsearch_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineLocalsearch *self = NAUTILUS_SEARCH_ENGINE_LOCALSEARCH (provider);

    if (self->query_pending)
    {
        g_debug ("Tracker engine stop");
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        self->query_pending = FALSE;
    }
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->start = search_engine_localsearch_start;
    iface->stop = nautilus_search_engine_localsearch_stop;
}

static void
nautilus_search_engine_localsearch_class_init (NautilusSearchEngineLocalsearchClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;
}

static void
nautilus_search_engine_localsearch_init (NautilusSearchEngineLocalsearch *engine)
{
    GError *error = NULL;

    engine->hits_pending = g_queue_new ();
    engine->statements = g_hash_table_new_full (NULL, NULL, NULL,
                                                g_object_unref);

    engine->connection = nautilus_localsearch_get_miner_fs_connection (&error);
    if (error)
    {
        g_warning ("Could not establish a connection to Tracker: %s", error->message);
        g_error_free (error);
    }
}


NautilusSearchEngineLocalsearch *
nautilus_search_engine_localsearch_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_LOCALSEARCH, NULL);
}
