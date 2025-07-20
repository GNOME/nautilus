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
    NautilusSearchProvider parent_instance;

    GtkRecentManager *recent_manager;
};

G_DEFINE_FINAL_TYPE (NautilusSearchEngineRecent,
                     nautilus_search_engine_recent,
                     NAUTILUS_TYPE_SEARCH_PROVIDER)

NautilusSearchEngineRecent *
nautilus_search_engine_recent_new (void)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_RECENT, NULL);
}

static void
nautilus_search_engine_recent_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_search_engine_recent_parent_class)->finalize (object);
}

static gboolean
is_file_valid_recursive (NautilusSearchEngineRecent  *self,
                         GFile                       *file,
                         gboolean                     show_hidden,
                         GDateTime                  **mtime,
                         GDateTime                  **atime,
                         GDateTime                  **ctime,
                         GError                     **error)
{
    g_autoptr (GFileInfo) file_info = NULL;

    file_info = g_file_query_info (file, FILE_ATTRIBS,
                                   G_FILE_QUERY_INFO_NONE,
                                   nautilus_search_provider_get_cancellable (self), error);
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

    if (!show_hidden)
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
                return is_file_valid_recursive (self, parent, show_hidden,
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
recent_thread_func (NautilusSearchEngineRecent *self)
{
    g_autoptr (GPtrArray) date_range = NULL;
    g_autoptr (GFile) query_location = NULL;
    GList *recent_items;
    NautilusQuery *query = nautilus_search_provider_get_query (self);
    gboolean show_hidden = nautilus_query_get_show_hidden_files (query);
    GList *l;

    recent_items = gtk_recent_manager_get_items (self->recent_manager);
    date_range = nautilus_query_get_date_range (query);
    query_location = nautilus_query_get_location (query);

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

        if (nautilus_search_provider_should_stop (self))
        {
            break;
        }

        name = gtk_recent_info_get_display_name (info);
        rank = nautilus_query_matches_string (query, name);

        if (rank <= 0)
        {
            g_autofree char *short_name = gtk_recent_info_get_short_name (info);
            rank = nautilus_query_matches_string (query, short_name);
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

            if (!is_file_valid_recursive (self, file, show_hidden, &mtime, &atime, &ctime, &error))
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

            const char *mime_type = gtk_recent_info_get_mime_type (info);

            if (!nautilus_query_matches_mime_type (query, mime_type))
            {
                continue;
            }

            if (date_range != NULL)
            {
                NautilusSearchTimeType type;
                GDateTime *target_date;
                GDateTime *initial_date;
                GDateTime *end_date;

                initial_date = g_ptr_array_index (date_range, 0);
                end_date = g_ptr_array_index (date_range, 1);
                type = nautilus_query_get_search_type (query);

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

            nautilus_search_provider_add_hit (self, hit);
        }
    }

    g_idle_add_once ((GSourceOnceFunc) nautilus_search_provider_finished, self);

    g_list_free_full (recent_items, (GDestroyNotify) gtk_recent_info_unref);

    return NULL;
}

static const char *
get_name (NautilusSearchProvider *provider)
{
    return "recent";
}

static gboolean
run_in_thread (NautilusSearchProvider *provider)
{
    return TRUE;
}

static void
start_search (NautilusSearchProvider *provider)
{
    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);
    g_autoptr (GThread) thread = NULL;

    thread = g_thread_new ("nautilus-search-recent", (GThreadFunc) recent_thread_func, self);
}
static void
nautilus_search_engine_recent_stop (NautilusSearchProvider *provider)
{
}

static void
nautilus_search_engine_recent_class_init (NautilusSearchEngineRecentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (klass);

    object_class->finalize = nautilus_search_engine_recent_finalize;
    search_provider_class->get_name = get_name;
    search_provider_class->run_in_thread = run_in_thread;
    search_provider_class->start_search = start_search;
    search_provider_class->stop = nautilus_search_engine_recent_stop;
}

static void
nautilus_search_engine_recent_init (NautilusSearchEngineRecent *self)
{
    self->recent_manager = gtk_recent_manager_get_default ();
}
