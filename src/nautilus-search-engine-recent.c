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
#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "nautilus-search-engine-recent.h"

#include "nautilus-query.h"
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define FILE_ATTRIBS G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
        G_FILE_ATTRIBUTE_ACCESS_CAN_READ "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
        G_FILE_ATTRIBUTE_TIME_ACCESS "," \
        G_FILE_ATTRIBUTE_TIME_CREATED

struct _NautilusSearchEngineRecent
{
    GObject parent_instance;

    NautilusQuery *query;
    gboolean running;
    GCancellable *cancellable;
    GtkRecentManager *recent_manager;
    GPtrArray *hits;
    guint add_hits_idle_id;
};

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineRecent,
                         nautilus_search_engine_recent,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

NautilusSearchEngineRecent *
nautilus_search_engine_recent_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_RECENT, NULL);
}

static void
nautilus_search_engine_recent_finalize (GObject *object)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (object);

    g_clear_handle_id (&self->add_hits_idle_id, g_source_remove);
    g_cancellable_cancel (self->cancellable);

    g_clear_object (&self->query);
    g_clear_object (&self->cancellable);
    g_clear_pointer (&self->hits, g_ptr_array_unref);

    G_OBJECT_CLASS (nautilus_search_engine_recent_parent_class)->finalize (object);
}

static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
    g_autoptr (NautilusSearchEngineRecent) self = user_data;
    NautilusSearchProvider *provider = NAUTILUS_SEARCH_PROVIDER (self);

    self->add_hits_idle_id = 0;
    if (self->hits->len > 0 &&
        !g_cancellable_is_cancelled (self->cancellable))
    {
        nautilus_search_provider_hits_added (provider, g_steal_pointer (&self->hits));
        g_debug ("Recent engine add hits");
    }

    self->running = FALSE;
    g_clear_object (&self->cancellable);
    g_clear_pointer (&self->hits, g_ptr_array_unref);

    g_debug ("Recent engine finished");
    nautilus_search_provider_finished (provider,
                                       NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);

    return FALSE;
}

static void
search_add_hits_idle (NautilusSearchEngineRecent *self,
                      GPtrArray                  *hits)
{
    if (self->add_hits_idle_id != 0)
    {
        g_clear_pointer (&hits, g_ptr_array_unref);

        return;
    }

    self->hits = hits;
    self->add_hits_idle_id = g_idle_add (search_thread_add_hits_idle, g_object_ref (self));
}

static gboolean
is_file_valid_recursive (NautilusSearchEngineRecent  *self,
                         GFile                       *file,
                         GDateTime                  **mtime,
                         GDateTime                  **atime,
                         GDateTime                  **ctime,
                         GError                     **error)
{
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

    if (mtime && atime && ctime)
    {
        *mtime = g_file_info_get_modification_date_time (file_info);
        *atime = g_file_info_get_access_date_time (file_info);
        *ctime = g_file_info_get_creation_date_time (file_info);
    }

    if (!nautilus_query_get_show_hidden_files (self->query))
    {
        gboolean is_hidden;

        is_hidden = g_file_info_get_attribute_boolean (file_info,
                                                       G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                    g_file_info_get_attribute_boolean (file_info,
                                                       G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);
        if (!is_hidden)
        {
            g_autoptr (GFile) parent = g_file_get_parent (file);

            if (parent)
            {
                return is_file_valid_recursive (self, parent,
                                                NULL, NULL, NULL,
                                                error);
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
    g_autoptr (NautilusSearchEngineRecent) self = NAUTILUS_SEARCH_ENGINE_RECENT (user_data);
    g_autoptr (GPtrArray) date_range = NULL;
    g_autoptr (GFile) query_location = NULL;
    g_autoptr (GPtrArray) mime_types = NULL;
    GList *recent_items;
    GPtrArray *hits = g_ptr_array_new_with_free_func (g_object_unref);
    GList *l;

    g_return_val_if_fail (self->query, NULL);

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

        if (query_location != NULL && !g_file_has_prefix (file, query_location))
        {
            continue;
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
            g_autoptr (GDateTime) mtime = NULL;
            g_autoptr (GDateTime) atime = NULL;
            g_autoptr (GDateTime) ctime = NULL;
            g_autoptr (GError) error = NULL;

            if (!gtk_recent_info_is_local (info))
            {
                continue;
            }

            if (!is_file_valid_recursive (self, file, &mtime, &atime, &ctime, &error))
            {
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                {
                    break;
                }

                if (error != NULL &&
                    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                {
                    g_debug ("Impossible to read recent file info: %s",
                             error->message);
                }

                continue;
            }

            if (mime_types->len > 0)
            {
                const gchar *mime_type = gtk_recent_info_get_mime_type (info);
                gboolean found = FALSE;

                for (guint i = 0; mime_type != NULL && i < mime_types->len; i++)
                {
                    if (g_content_type_is_a (mime_type, g_ptr_array_index (mime_types, i)))
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

            if (date_range != NULL)
            {
                NautilusSearchTimeType type;
                GDateTime *target_date;
                GDateTime *initial_date;
                GDateTime *end_date;

                initial_date = g_ptr_array_index (date_range, 0);
                end_date = g_ptr_array_index (date_range, 1);
                type = nautilus_query_get_search_type (self->query);

                switch (type)
                {
                    case NAUTILUS_SEARCH_TIME_TYPE_LAST_ACCESS:
                    {
                        target_date = atime;
                    }
                    break;

                    case NAUTILUS_SEARCH_TIME_TYPE_LAST_MODIFIED:
                    {
                        target_date = mtime;
                    }
                    break;

                    case NAUTILUS_SEARCH_TIME_TYPE_CREATED:
                    {
                        target_date = ctime;
                    }
                    break;

                    default:
                    {
                        target_date = NULL;
                    }
                }

                if (!nautilus_date_time_is_between_dates (target_date,
                                                          initial_date,
                                                          end_date))
                {
                    continue;
                }
            }

            hit = nautilus_search_hit_new (uri);
            nautilus_search_hit_set_fts_rank (hit, rank);
            nautilus_search_hit_set_modification_time (hit, mtime);
            nautilus_search_hit_set_access_time (hit, atime);
            nautilus_search_hit_set_creation_time (hit, ctime);

            g_ptr_array_add (hits, hit);
        }
    }

    search_add_hits_idle (self, hits);

    g_list_free_full (recent_items, (GDestroyNotify) gtk_recent_info_unref);

    return NULL;
}

static gboolean
search_engine_recent_start (NautilusSearchProvider *provider,
                            NautilusQuery          *query)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);
    g_autoptr (GThread) thread = NULL;

    g_return_val_if_fail (self->cancellable == NULL, FALSE);

    g_set_object (&self->query, query);
    g_debug ("Recent engine start");

    self->running = TRUE;
    self->cancellable = g_cancellable_new ();
    thread = g_thread_new ("nautilus-search-recent", recent_thread_func,
                           g_object_ref (self));

    return TRUE;
}
static void
nautilus_search_engine_recent_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);

    if (self->cancellable != NULL)
    {
        g_debug ("Recent engine stop");
        g_cancellable_cancel (self->cancellable);
    }

    self->running = FALSE;
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->start = search_engine_recent_start;
    iface->stop = nautilus_search_engine_recent_stop;
}

static void
nautilus_search_engine_recent_class_init (NautilusSearchEngineRecentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_search_engine_recent_finalize;
}

static void
nautilus_search_engine_recent_init (NautilusSearchEngineRecent *self)
{
    self->recent_manager = gtk_recent_manager_get_default ();
}
