/*
 * Copyright (C) 2005 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */
#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "nautilus-search-engine-simple.h"

#include "nautilus-query.h"
#include "nautilus-recursive-directory-iterator.h"
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define FLUSH_TIME_SPAN (250 * G_TIME_SPAN_MILLISECOND)

struct _NautilusSearchEngineSimple
{
    NautilusSearchProvider parent_instance;

    gint64 last_saved_time;
};

G_DEFINE_FINAL_TYPE (NautilusSearchEngineSimple,
                     nautilus_search_engine_simple,
                     NAUTILUS_TYPE_SEARCH_PROVIDER)

#define STD_ATTRIBUTES \
        G_FILE_ATTRIBUTE_STANDARD_NAME "," \
        G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
        G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
        G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
        G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
        G_FILE_ATTRIBUTE_TIME_ACCESS "," \
        G_FILE_ATTRIBUTE_TIME_CREATED "," \
        G_FILE_ATTRIBUTE_ID_FILE

#define STD_ATTRIBUTES_WITH_CONTENT_TYPE \
        STD_ATTRIBUTES "," \
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
        G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE

#define STD_ATTRIBUTES_LOCAL_ONLY \
        STD_ATTRIBUTES "," \
        G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE

#define STD_ATTRIBUTES_WITH_CONTENT_TYPE_LOCAL_ONLY \
        STD_ATTRIBUTES_WITH_CONTENT_TYPE "," \
        G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE

static void
iterate_file (GFileInfo                  *info,
              GFile                      *location,
              NautilusSearchEngineSimple *self)
{
    NautilusQuery *query = nautilus_search_provider_get_query (self);

    NautilusSearchTimeType type = nautilus_query_get_search_type (query);
    g_autoptr (GPtrArray) date_range = nautilus_query_get_date_range (query);
    gboolean show_hidden = nautilus_query_get_show_hidden_files (query);
    const char *display_name = g_file_info_get_display_name (info);

    if (display_name == NULL)
    {
        return;
    }

    if (!show_hidden &&
        (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
         g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP)))
    {
        return;
    }

    gdouble match = nautilus_query_matches_string (query, display_name);
    gboolean found = (match > -1);
    gint64 current_time;

    if (found && nautilus_query_has_mime_types (query))
    {
        const char *mime_type = g_file_info_get_attribute_string (
            info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
        if (mime_type == NULL)
        {
            mime_type = g_file_info_get_attribute_string (info,
                                                          G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
        }

        found = nautilus_query_matches_mime_type (query, mime_type);
    }

    g_autoptr (GDateTime) mtime = g_file_info_get_modification_date_time (info);
    g_autoptr (GDateTime) atime = g_file_info_get_access_date_time (info);
    g_autoptr (GDateTime) ctime = g_file_info_get_creation_date_time (info);

    if (found && date_range != NULL)
    {
        GDateTime *target_date;
        GDateTime *initial_date = g_ptr_array_index (date_range, 0);
        GDateTime *end_date = g_ptr_array_index (date_range, 1);

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

        found = nautilus_date_time_is_between_dates (target_date,
                                                     initial_date,
                                                     end_date);
    }

    if (found)
    {
        g_autofree gchar *uri = g_file_get_uri (location);
        NautilusSearchHit *hit = nautilus_search_hit_new (uri);

        nautilus_search_hit_set_fts_rank (hit, match);
        nautilus_search_hit_set_modification_time (hit, mtime);
        nautilus_search_hit_set_access_time (hit, atime);
        nautilus_search_hit_set_creation_time (hit, ctime);

        nautilus_search_provider_add_hit (self, hit);
    }

    current_time = g_get_monotonic_time ();
    if (current_time - self->last_saved_time >= FLUSH_TIME_SPAN)
    {
        self->last_saved_time = current_time;
        nautilus_search_provider_flush_hits (self);
    }
}

static void
iterate_finish (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
    NautilusSearchEngineSimple *self = user_data;

    g_idle_add_once ((GSourceOnceFunc) nautilus_search_provider_finished, self);
}

static const char *
get_name (NautilusSearchProvider *provider)
{
    return "simple";
}

static gboolean
run_in_thread (NautilusSearchProvider *provider)
{
    return TRUE;
}

static guint
search_delay (NautilusSearchProvider *provider)
{
    return 500;
}

static gboolean
should_search (NautilusSearchProvider *provider,
               NautilusQuery          *query)
{
    g_autoptr (GFile) location = nautilus_query_get_location (query);

    return location != NULL;
}

static void
start_search (NautilusSearchProvider *provider)
{
    NautilusSearchEngineSimple *self = NAUTILUS_SEARCH_ENGINE_SIMPLE (provider);

    GCancellable *cancellable = nautilus_search_provider_get_cancellable (self);
    NautilusQuery *query = nautilus_search_provider_get_query (self);
    g_autoptr (GFile) toplevel = nautilus_query_get_location (query);
    gboolean local_only = nautilus_query_recursive_local_only (query);
    const char *attributes = nautilus_query_has_mime_types (query)
                             ? (local_only ? STD_ATTRIBUTES_WITH_CONTENT_TYPE_LOCAL_ONLY
                                           : STD_ATTRIBUTES_WITH_CONTENT_TYPE)
                             : (local_only ? STD_ATTRIBUTES_LOCAL_ONLY : STD_ATTRIBUTES);
    gboolean recursion_enabled = nautilus_query_recursive (query);

    nautilus_iterate_directory_recursive (toplevel,
                                          recursion_enabled ? G_MAXUINT : 0,
                                          attributes,
                                          local_only,
                                          cancellable,
                                          (IterationFileCallback) iterate_file,
                                          iterate_finish,
                                          self);
}

static void
nautilus_search_engine_simple_class_init (NautilusSearchEngineSimpleClass *class)
{
    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (class);

    search_provider_class->get_name = get_name;
    search_provider_class->run_in_thread = run_in_thread;
    search_provider_class->search_delay = search_delay;
    search_provider_class->should_search = should_search;
    search_provider_class->start_search = start_search;
}

static void
nautilus_search_engine_simple_init (NautilusSearchEngineSimple *self)
{
    self->last_saved_time = g_get_monotonic_time ();
}

NautilusSearchEngineSimple *
nautilus_search_engine_simple_new (void)
{
    NautilusSearchEngineSimple *engine;

    engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

    return engine;
}
