/*
 * Copyright (C) 2018 Canonical Ltd
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
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
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include <config.h>
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine-recent.h"
#include "nautilus-search-engine-private.h"
#include "nautilus-ui-utilities.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define FILE_ATTRIBS G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
                     G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
                     G_FILE_ATTRIBUTE_ACCESS_CAN_READ ","

struct _NautilusSearchEngineRecent
{
    GObject parent_instance;

    NautilusQuery *query;
    GCancellable *cancellable;
    GtkRecentManager *recent_manager;
    guint add_hits_idle_id;
};

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineRecent,
                         nautilus_search_engine_recent,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

enum
{
  PROP_0,
  PROP_RUNNING,
  LAST_PROP
};


NautilusSearchEngineRecent *
nautilus_search_engine_recent_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_RECENT, NULL);
}

static void
nautilus_search_engine_recent_finalize (GObject *object)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (object);

    if (self->add_hits_idle_id != 0)
    {
        g_source_remove (self->add_hits_idle_id);
    }

    g_cancellable_cancel (self->cancellable);

    g_clear_object (&self->query);
    g_clear_object (&self->cancellable);

    G_OBJECT_CLASS (nautilus_search_engine_recent_parent_class)->finalize (object);
}

typedef struct
{
    NautilusSearchEngineRecent *recent;
    GList *hits;
} SearchHitsData;


static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
    SearchHitsData *search_hits = user_data;
    NautilusSearchEngineRecent *self = search_hits->recent;
    NautilusSearchProvider *provider = NAUTILUS_SEARCH_PROVIDER (self);

    self->add_hits_idle_id = 0;

    if (!g_cancellable_is_cancelled (self->cancellable))
    {
        nautilus_search_provider_hits_added (provider, search_hits->hits);
        DEBUG ("Recent engine add hits");
    }

    g_list_free_full (search_hits->hits, g_object_unref);
    g_clear_object (&self->cancellable);
    g_free (search_hits);

    nautilus_search_provider_finished (provider,
                                       NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);
    g_object_notify (G_OBJECT (provider), "running");

    g_object_unref (self);

    return FALSE;
}

static void
search_add_hits_idle (NautilusSearchEngineRecent *self,
                      GList                      *hits)
{
    SearchHitsData *search_hits;

    if (self->add_hits_idle_id != 0)
    {
        return;
    }

    search_hits = g_new0 (SearchHitsData, 1);
    search_hits->recent = self;
    search_hits->hits = hits;

    self->add_hits_idle_id = g_idle_add (search_thread_add_hits_idle, search_hits);
}

static gboolean
is_file_valid_recursive (NautilusSearchEngineRecent *self,
                         GFile                      *file,
                         GError                    **error)
{
    g_autofree gchar *path = NULL;
    g_autoptr (GFileInfo) file_info = NULL;

    file_info = g_file_query_info (file, FILE_ATTRIBS,
                                   G_FILE_QUERY_INFO_NONE,
                                   self->cancellable, error);
    if (*error != NULL)
    {
        return FALSE;
    }

    if (!g_file_info_get_attribute_boolean (file_info,
                                            G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        return FALSE;
    }

    path = g_file_get_path (file);

    if (!nautilus_query_get_show_hidden_files (self->query))
    {
        if (!g_file_info_get_is_hidden (file_info) &&
            !g_file_info_get_is_backup (file_info))
        {
            g_autoptr (GFile) parent = g_file_get_parent (file);

            if (parent)
            {
                return is_file_valid_recursive (self, parent, error);
            }
        }
        else
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gpointer
recent_thread_func (gpointer user_data)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (user_data);
    g_autoptr (GPtrArray) date_range = NULL;
    g_autoptr (GFile) query_location = NULL;
    SearchHitsData *search_hits;
    GList *recent_items;
    GList *mime_types;
    GList *hits;
    GList *l;

    g_return_val_if_fail (self->query, NULL);

    hits = NULL;
    recent_items = gtk_recent_manager_get_items (self->recent_manager);
    mime_types = nautilus_query_get_mime_types (self->query);
    date_range = nautilus_query_get_date_range (self->query);
    query_location = nautilus_query_get_location (self->query);

    for (l = recent_items; l != NULL; l = l->next)
    {
        GtkRecentInfo *info = l->data;
        g_autoptr (GFile) file = NULL;
        const gchar *uri;
        const gchar *name;
        gdouble rank;

        uri = gtk_recent_info_get_uri (info);
        file = g_file_new_for_uri (uri);

        if (!g_file_has_prefix (file, query_location))
        {
            continue;
        }

        if (gtk_recent_info_is_local (info))
        {
            g_autoptr (GError) error = NULL;

            if (!is_file_valid_recursive (self, file, &error))
            {
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                {
                    break;
                }

                if (error != NULL &&
                    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                {
                    g_debug("Impossible to read recent file info: %s",
                            error->message);
                }

                continue;
            }
        }

        if (g_cancellable_is_cancelled (self->cancellable))
        {
            break;
        }

        name = gtk_recent_info_get_display_name (info);
        rank = nautilus_query_matches_string (self->query, name);

        if (rank <= 0)
        {
            g_autofree char *short_name = gtk_recent_info_get_short_name (info);
            rank = nautilus_query_matches_string (self->query, short_name);
        }

        if (rank > 0)
        {
            NautilusSearchHit *hit;
            time_t modified, visited;
            g_autoptr (GDateTime) gmodified = NULL;
            g_autoptr (GDateTime) gvisited = NULL;

            if (mime_types)
            {
                GList *ml;
                const gchar *mime_type = gtk_recent_info_get_mime_type (info);
                gboolean found = FALSE;

                for (ml = mime_types; mime_type != NULL && ml != NULL;
                     ml = ml->next)
                {
                    if (g_content_type_is_a (mime_type, ml->data))
                    {
                        found = TRUE;
                        break;
                    }
                }

                if (!found)
                {
                    continue;
                }
            }

            modified = gtk_recent_info_get_modified (info);
            visited = gtk_recent_info_get_visited (info);

            gmodified = g_date_time_new_from_unix_local (modified);
            gvisited = g_date_time_new_from_unix_local (visited);

            if (date_range != NULL)
            {
                NautilusQuerySearchType type;
                guint64 target_time;
                GDateTime *initial_date;
                GDateTime *end_date;

                initial_date = g_ptr_array_index (date_range, 0);
                end_date = g_ptr_array_index (date_range, 1);
                type = nautilus_query_get_search_type (self->query);
                target_time = (type == NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS) ?
                               visited : modified;

                if (!nautilus_file_date_in_between (target_time,
                                                    initial_date, end_date))
                {
                    continue;
                }
            }

            hit = nautilus_search_hit_new (uri);
            nautilus_search_hit_set_fts_rank (hit, rank);
            nautilus_search_hit_set_modification_time (hit, gmodified);
            nautilus_search_hit_set_access_time (hit, gvisited);

            hits = g_list_prepend (hits, hit);
        }
    }

    search_add_hits_idle (self, hits);

    g_list_free_full (recent_items, (GDestroyNotify) gtk_recent_info_unref);
    g_list_free_full (mime_types, g_free);

    return NULL;
}

static void
nautilus_search_engine_recent_start (NautilusSearchProvider *provider)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);
    g_autoptr (GFile) location = NULL;
    GThread *thread;

    g_return_if_fail (self->query);
    g_return_if_fail (self->cancellable == NULL);

    location = nautilus_query_get_location (self->query);

    if (!is_recursive_search (NAUTILUS_SEARCH_ENGINE_TYPE_INDEXED,
                              nautilus_query_get_recursive (self->query),
                              location))
    {
        search_add_hits_idle (self, NULL);
        return;
    }

    g_object_ref (self);
    self->cancellable = g_cancellable_new ();

    thread = g_thread_new ("nautilus-search-recent", recent_thread_func, self);
    g_object_notify (G_OBJECT (provider), "running");

    g_thread_unref (thread);
}

static void
nautilus_search_engine_recent_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);

    if (self->cancellable != NULL)
    {
        DEBUG ("Recent engine stop");
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

        g_object_notify (G_OBJECT (provider), "running");
    }
}

static void
nautilus_search_engine_recent_set_query (NautilusSearchProvider *provider,
                                         NautilusQuery          *query)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);

    g_clear_object (&self->query);
    self->query = g_object_ref (query);
}

static gboolean
nautilus_search_engine_recent_is_running (NautilusSearchProvider *provider)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);

    return self->cancellable != NULL &&
           !g_cancellable_is_cancelled (self->cancellable);
}

static void
nautilus_search_engine_recent_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
    NautilusSearchProvider *provider = NAUTILUS_SEARCH_PROVIDER (object);

    switch (prop_id)
    {
        case PROP_RUNNING:
        {
            gboolean running;
            running = nautilus_search_engine_recent_is_running (provider);
            g_value_set_boolean (value, running);
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->set_query = nautilus_search_engine_recent_set_query;
    iface->start = nautilus_search_engine_recent_start;
    iface->stop = nautilus_search_engine_recent_stop;
    iface->is_running = nautilus_search_engine_recent_is_running;
}

static void
nautilus_search_engine_recent_class_init (NautilusSearchEngineRecentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_search_engine_recent_finalize;
    object_class->get_property = nautilus_search_engine_recent_get_property;

    g_object_class_override_property (object_class, PROP_RUNNING, "running");
}

static void
nautilus_search_engine_recent_init (NautilusSearchEngineRecent *self)
{
    self->recent_manager = gtk_recent_manager_get_default ();
}
