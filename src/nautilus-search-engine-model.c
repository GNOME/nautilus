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

#include <config.h>
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-directory.h"
#include "nautilus-directory-private.h"
#include "nautilus-file.h"
#include "nautilus-ui-utilities.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct _NautilusSearchEngineModel
{
    GObject parent;

    NautilusQuery *query;

    GList *hits;
    NautilusDirectory *directory;

    gboolean query_pending;
    guint finished_id;
};

enum
{
    PROP_0,
    PROP_RUNNING,
    LAST_PROP
};

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineModel,
                         nautilus_search_engine_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

static void
finalize (GObject *object)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (object);

    if (model->hits != NULL)
    {
        g_list_free_full (model->hits, g_object_unref);
        model->hits = NULL;
    }

    if (model->finished_id != 0)
    {
        g_source_remove (model->finished_id);
        model->finished_id = 0;
    }

    g_clear_object (&model->directory);
    g_clear_object (&model->query);

    G_OBJECT_CLASS (nautilus_search_engine_model_parent_class)->finalize (object);
}

static gboolean
search_finished (NautilusSearchEngineModel *model)
{
    model->finished_id = 0;

    if (model->hits != NULL)
    {
        DEBUG ("Model engine hits added");
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (model),
                                             model->hits);
        g_list_free_full (model->hits, g_object_unref);
        model->hits = NULL;
    }

    model->query_pending = FALSE;

    g_object_notify (G_OBJECT (model), "running");

    DEBUG ("Model engine finished");
    nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (model),
                                       NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);
    g_object_unref (model);

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
    g_autoptr (GPtrArray) mime_types = NULL;
    gchar *uri, *display_name;
    GList *files, *hits, *l;
    NautilusFile *file;
    gdouble match;
    gboolean found;
    NautilusSearchHit *hit;
    GDateTime *initial_date;
    GDateTime *end_date;
    GPtrArray *date_range;

    files = nautilus_directory_get_file_list (directory);
    mime_types = nautilus_query_get_mime_types (model->query);
    hits = NULL;

    for (l = files; l != NULL; l = l->next)
    {
        g_autoptr (GDateTime) mtime = NULL;
        g_autoptr (GDateTime) atime = NULL;
        g_autoptr (GDateTime) ctime = NULL;

        file = l->data;

        display_name = nautilus_file_get_display_name (file);
        match = nautilus_query_matches_string (model->query, display_name);
        found = (match > -1);

        if (found && mime_types->len > 0)
        {
            found = FALSE;

            for (gint i = 0; i < mime_types->len; i++)
            {
                if (nautilus_file_is_mime_type (file, g_ptr_array_index (mime_types, i)))
                {
                    found = TRUE;
                    break;
                }
            }
        }

        mtime = g_date_time_new_from_unix_local (nautilus_file_get_mtime (file));
        atime = g_date_time_new_from_unix_local (nautilus_file_get_atime (file));
        ctime = g_date_time_new_from_unix_local (nautilus_file_get_btime (file));

        date_range = nautilus_query_get_date_range (model->query);
        if (found && date_range != NULL)
        {
            NautilusQuerySearchType type;
            GDateTime *target_date;

            type = nautilus_query_get_search_type (model->query);
            initial_date = g_ptr_array_index (date_range, 0);
            end_date = g_ptr_array_index (date_range, 1);

            if (type == NAUTILUS_QUERY_SEARCH_TYPE_LAST_ACCESS)
            {
                target_date = atime;
            }
            else if (type == NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED)
            {
                target_date = mtime;
            }
            else
            {
                target_date = ctime;
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

            hits = g_list_prepend (hits, hit);

            g_free (uri);
        }

        g_free (display_name);
    }

    nautilus_file_list_free (files);
    model->hits = hits;

    search_finished (model);
}

static void
nautilus_search_engine_model_start (NautilusSearchProvider *provider)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

    if (model->query_pending)
    {
        return;
    }

    DEBUG ("Model engine start");

    g_object_ref (model);
    model->query_pending = TRUE;

    g_object_notify (G_OBJECT (provider), "running");

    if (model->directory == NULL)
    {
        search_finished_idle (model);
        return;
    }

    nautilus_directory_call_when_ready (model->directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        TRUE, model_directory_ready_cb, model);
}

static void
nautilus_search_engine_model_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

    if (model->query_pending)
    {
        DEBUG ("Model engine stop");

        nautilus_directory_cancel_callback (model->directory,
                                            model_directory_ready_cb, model);
        search_finished_idle (model);
    }

    g_clear_object (&model->directory);
}

static void
nautilus_search_engine_model_set_query (NautilusSearchProvider *provider,
                                        NautilusQuery          *query)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

    g_object_ref (query);
    g_clear_object (&model->query);
    model->query = query;
}

static gboolean
nautilus_search_engine_model_is_running (NautilusSearchProvider *provider)
{
    NautilusSearchEngineModel *model;

    model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

    return model->query_pending;
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->set_query = nautilus_search_engine_model_set_query;
    iface->start = nautilus_search_engine_model_start;
    iface->stop = nautilus_search_engine_model_stop;
    iface->is_running = nautilus_search_engine_model_is_running;
}

static void
nautilus_search_engine_model_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
    NautilusSearchProvider *self = NAUTILUS_SEARCH_PROVIDER (object);

    switch (prop_id)
    {
        case PROP_RUNNING:
        {
            g_value_set_boolean (value, nautilus_search_engine_model_is_running (self));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_search_engine_model_class_init (NautilusSearchEngineModelClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = finalize;
    gobject_class->get_property = nautilus_search_engine_model_get_property;

    /**
     * NautilusSearchEngine::running:
     *
     * Whether the search engine is running a search.
     */
    g_object_class_override_property (gobject_class, PROP_RUNNING, "running");
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

void
nautilus_search_engine_model_set_model (NautilusSearchEngineModel *model,
                                        NautilusDirectory         *directory)
{
    g_clear_object (&model->directory);
    model->directory = nautilus_directory_ref (directory);
}

NautilusDirectory *
nautilus_search_engine_model_get_model (NautilusSearchEngineModel *model)
{
    return model->directory;
}
