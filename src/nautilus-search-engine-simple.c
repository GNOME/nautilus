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
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define BATCH_SIZE 500

struct _NautilusSearchEngineSimple
{
    NautilusSearchProvider parent_instance;

    GQueue *directories;     /* GFiles */

    GHashTable *visited;
};

G_DEFINE_FINAL_TYPE (NautilusSearchEngineSimple,
                     nautilus_search_engine_simple,
                     NAUTILUS_TYPE_SEARCH_PROVIDER)

static void
finalize (GObject *object)
{
    NautilusSearchEngineSimple *self = NAUTILUS_SEARCH_ENGINE_SIMPLE (object);

    g_queue_free_full (self->directories, (GDestroyNotify) g_object_unref);
    g_hash_table_destroy (self->visited);

    G_OBJECT_CLASS (nautilus_search_engine_simple_parent_class)->finalize (object);
}

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

static gboolean
file_is_remote (GFile *file)
{
    g_autoptr (GFileInfo) file_system_info = g_file_query_filesystem_info (
        file, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE, NULL, NULL);

    return file_system_info != NULL &&
           g_file_info_get_attribute_boolean (file_system_info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE);
}

static void
visit_directory (GFile                      *dir,
                 NautilusSearchEngineSimple *self)
{
    NautilusQuery *query = nautilus_search_provider_get_query (self);
    const char *attributes = nautilus_query_has_mime_types (query)
                             ? STD_ATTRIBUTES_WITH_CONTENT_TYPE : STD_ATTRIBUTES;
    GCancellable *cancellable = nautilus_search_provider_get_cancellable (self);

    g_autoptr (GFileEnumerator) enumerator = g_file_enumerate_children (
        dir,
        attributes,
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
        cancellable, NULL);

    if (enumerator == NULL)
    {
        return;
    }

    NautilusSearchTimeType type = nautilus_query_get_search_type (query);
    g_autoptr (GPtrArray) date_range = nautilus_query_get_date_range (query);
    gboolean show_hidden = nautilus_query_get_show_hidden_files (query);
    gboolean recursion_enabled = nautilus_query_recursive (query);
    gboolean per_location_recursive_check = nautilus_query_recursive_local_only (query);

    GFileInfo *info;
    while (g_file_enumerator_iterate (enumerator, &info, NULL, cancellable, NULL) &&
           info != NULL)
    {
        const char *display_name = g_file_info_get_display_name (info);

        if (display_name == NULL)
        {
            continue;
        }

        if (!show_hidden &&
            (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
             g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP)))
        {
            continue;
        }

        g_autoptr (GFile) child = g_file_get_child (dir, g_file_info_get_name (info));
        gdouble match = nautilus_query_matches_string (query, display_name);
        gboolean found = (match > -1);

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
            g_autofree gchar *uri = g_file_get_uri (child);
            NautilusSearchHit *hit = nautilus_search_hit_new (uri);

            nautilus_search_hit_set_fts_rank (hit, match);
            nautilus_search_hit_set_modification_time (hit, mtime);
            nautilus_search_hit_set_access_time (hit, atime);
            nautilus_search_hit_set_creation_time (hit, ctime);

            nautilus_search_provider_add_hit (self, hit);
        }

        if (recursion_enabled &&
            g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY &&
            (!per_location_recursive_check || !file_is_remote (child)))
        {
            const char *id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
            if (id != NULL &&
                !g_hash_table_contains (self->visited, id))
            {
                g_hash_table_add (self->visited, g_strdup (id));
                g_queue_push_tail (self->directories, g_steal_pointer (&child));
            }
        }
    }
}


static gpointer
search_thread_func (NautilusSearchEngineSimple *self)
{
    GCancellable *cancellable = nautilus_search_provider_get_cancellable (self);
    NautilusQuery *query = nautilus_search_provider_get_query (self);

    /* Insert id for toplevel directory into visited */
    g_autoptr (GFile) toplevel = nautilus_query_get_location (query);
    g_autoptr (GFileInfo) info = g_file_query_info (
        toplevel, G_FILE_ATTRIBUTE_ID_FILE, 0, cancellable, NULL);

    if (info != NULL)
    {
        const char *id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);

        if (id != NULL)
        {
            g_hash_table_insert (self->visited, g_strdup (id), NULL);
        }
    }

    visit_directory (toplevel, self);

    while (!nautilus_search_provider_should_stop (self))
    {
        g_autoptr (GFile) dir = g_queue_pop_head (self->directories);

        if (dir == NULL)
        {
            break;
        }

        visit_directory (dir, self);
    }

    g_queue_clear_full (self->directories, (GDestroyNotify) g_object_unref);
    g_hash_table_remove_all (self->visited);

    g_idle_add_once ((GSourceOnceFunc) nautilus_search_provider_finished, self);

    return NULL;
}

static void
create_thread (gpointer user_data)
{
    NautilusSearchEngineSimple *simple = user_data;
    g_autoptr (GThread) thread = NULL;

    thread = g_thread_new ("nautilus-search-simple", (GThreadFunc) search_thread_func, simple);
}

static const char*
get_name (NautilusSearchProvider *provider)
{
    return "simple";
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

    g_assert_true (g_queue_is_empty (self->directories));
    g_assert_true (g_hash_table_size (self->visited) == 0);

    NautilusQuery *query = nautilus_search_provider_get_query (provider);
    g_queue_push_tail (self->directories, nautilus_query_get_location (query));

    create_thread (self);
}

static void
nautilus_search_engine_simple_stop (NautilusSearchProvider *provider)
{
}

static void
nautilus_search_engine_simple_class_init (NautilusSearchEngineSimpleClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;

    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (class);
    search_provider_class->get_name = get_name;
    search_provider_class->search_delay = search_delay;
    search_provider_class->should_search = should_search;
    search_provider_class->start_search = start_search;
    search_provider_class->stop = nautilus_search_engine_simple_stop;
}

static void
nautilus_search_engine_simple_init (NautilusSearchEngineSimple *self)
{
    self->directories = g_queue_new ();
    self->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

NautilusSearchEngineSimple *
nautilus_search_engine_simple_new (void)
{
    NautilusSearchEngineSimple *engine;

    engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

    return engine;
}
