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
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-directory.h"
#include "nautilus-directory-private.h"
#include "nautilus-file.h"
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct _NautilusSearchEngineModel
{
    NautilusSearchProvider parent_instance;

    GPtrArray *hits;
    NautilusDirectory *directory;

    guint finished_id;
};

G_DEFINE_FINAL_TYPE (NautilusSearchEngineModel,
                     nautilus_search_engine_model,
                     NAUTILUS_TYPE_SEARCH_PROVIDER)

static void
finalize (GObject *object)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (object);

    g_clear_pointer (&model->hits, g_ptr_array_unref);

    if (model->finished_id != 0)
    {
        g_source_remove (model->finished_id);
        model->finished_id = 0;
    }

    g_clear_object (&model->directory);

    G_OBJECT_CLASS (nautilus_search_engine_model_parent_class)->finalize (object);
}

static gboolean
search_finished (NautilusSearchEngineModel *model)
{
    g_autoptr (GPtrArray) hits = g_steal_pointer (&model->hits);
    model->finished_id = 0;

    if (hits != NULL && hits->len > 0)
    {
        g_debug ("Model engine hits added");
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (model),
                                             g_steal_pointer (&hits));
    }

    g_debug ("Model engine finished");
    nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (model));

    return FALSE;
}

static void
search_finished_idle (NautilusSearchEngineModel *model)
{
    if (model->finished_id != 0)
    {
        return;
    }

    model->finished_id = g_idle_add ((GSourceFunc) search_finished, model);
}

static void
model_directory_ready_cb (NautilusDirectory *directory,
                          GList             *list,
                          gpointer           user_data)
{
    NautilusSearchEngineModel *model = user_data;
    gchar *uri;
    GList *files, *l;
    GPtrArray *hits = g_ptr_array_new_with_free_func (g_object_unref);
    NautilusFile *file;
    gdouble match;
    gboolean found;
    NautilusSearchHit *hit;
    GDateTime *initial_date;
    GDateTime *end_date;
    GPtrArray *date_range;

    NautilusQuery *query = nautilus_search_provider_get_query (model);

    files = nautilus_directory_get_file_list (directory);

    for (l = files; l != NULL; l = l->next)
    {
        g_autofree gchar *display_name = NULL;
        g_autoptr (GDateTime) mtime = NULL;
        g_autoptr (GDateTime) atime = NULL;
        g_autoptr (GDateTime) ctime = NULL;

        file = l->data;

        match = nautilus_query_matches_string (query,
                                               nautilus_file_get_display_name (file));
        found = (match > -1);
        if (!found)
        {
            continue;
        }

        const char *mime_type = nautilus_file_get_mime_type (file);
        if (!nautilus_query_matches_mime_type (query, mime_type))
        {
            continue;
        }

        mtime = g_date_time_new_from_unix_local (nautilus_file_get_mtime (file));
        atime = g_date_time_new_from_unix_local (nautilus_file_get_atime (file));
        ctime = g_date_time_new_from_unix_local (nautilus_file_get_btime (file));

        date_range = nautilus_query_get_date_range (query);
        if (found && date_range != NULL)
        {
            NautilusSearchTimeType type;
            GDateTime *target_date;

            type = nautilus_query_get_search_type (query);
            initial_date = g_ptr_array_index (date_range, 0);
            end_date = g_ptr_array_index (date_range, 1);

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
            g_ptr_array_unref (date_range);
        }

        if (found)
        {
            uri = nautilus_file_get_uri (file);
            hit = nautilus_search_hit_new (uri);
            nautilus_search_hit_set_fts_rank (hit, match);
            nautilus_search_hit_set_modification_time (hit, mtime);
            nautilus_search_hit_set_access_time (hit, atime);
            nautilus_search_hit_set_creation_time (hit, ctime);

            g_ptr_array_add (hits, hit);

            g_free (uri);
        }
    }

    nautilus_file_list_free (files);
    model->hits = hits;

    search_finished (model);
}

static gboolean
should_search (NautilusSearchProvider *provider,
               NautilusQuery          *query)
{
    g_return_val_if_fail (NAUTILUS_IS_SEARCH_ENGINE_MODEL (provider), FALSE);
    g_return_val_if_fail (nautilus_search_provider_should_stop (provider), FALSE);

    g_autoptr (GFile) location = nautilus_query_get_location (query);
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get (location);

    NautilusSearchEngineModel *self = NAUTILUS_SEARCH_ENGINE_MODEL (provider);
    g_set_object (&self->directory, directory);

    return directory != NULL;
}

static void
start_search (NautilusSearchProvider *provider)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

    g_debug ("Model engine start");

    nautilus_directory_call_when_ready (model->directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        TRUE, model_directory_ready_cb, model);
}

static void
nautilus_search_engine_model_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

    g_debug ("Model engine stop");

    nautilus_directory_cancel_callback (model->directory,
                                        model_directory_ready_cb, model);
    search_finished_idle (model);

    g_clear_object (&model->directory);
}

static void
nautilus_search_engine_model_class_init (NautilusSearchEngineModelClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;

    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (class);
    search_provider_class->should_search = should_search;
    search_provider_class->start_search = start_search;
    search_provider_class->stop = nautilus_search_engine_model_stop;
}

static void
nautilus_search_engine_model_init (NautilusSearchEngineModel *engine)
{
}

NautilusSearchEngineModel *
nautilus_search_engine_model_new (void)
{
    NautilusSearchEngineModel *engine;

    engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_MODEL, NULL);

    return engine;
}
