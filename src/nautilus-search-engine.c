/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */
#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "nautilus-search-engine.h"

#include "nautilus-file-utilities.h"
#include "nautilus-query.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-search-engine-localsearch.h"
#include "nautilus-search-engine-recent.h"
#include "nautilus-search-engine-simple.h"
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"

#include <glib/gi18n.h>

struct _NautilusSearchEngine
{
    GObject parent_instance;

    NautilusSearchType search_type;

    NautilusSearchProvider *localsearch;
    NautilusSearchProvider *model;
    NautilusSearchProvider *recent;
    NautilusSearchProvider *simple;

    GHashTable *uris;
    guint providers_running;
    guint providers_finished;
    guint providers_error;

    NautilusQuery *query;
    gboolean running;
    gboolean restart;
};

enum
{
    PROP_0,
    PROP_RUNNING,
    PROP_SEARCH_TYPE,
    N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

static void nautilus_search_provider_init (NautilusSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngine,
                         nautilus_search_engine,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
                                                nautilus_search_provider_init))

static void
search_engine_start_real_setup (NautilusSearchEngine *self)
{
    self->providers_running = 0;
    self->providers_finished = 0;
    self->providers_error = 0;

    self->restart = FALSE;

    g_debug ("Search engine start real setup");

    g_object_ref (self);
}

static void
search_engine_start_provider (NautilusSearchProvider *provider,
                              NautilusSearchEngine   *self)
{
    if (provider == NULL)
    {
        return;
    }
    else if (nautilus_search_provider_start (provider, self->query))
    {
        self->providers_running++;
    }
}

static void
search_engine_start_real (NautilusSearchEngine *self)
{
    search_engine_start_real_setup (self);

    search_engine_start_provider (self->localsearch, self);
    search_engine_start_provider (self->model, self);
    search_engine_start_provider (self->recent, self);
    search_engine_start_provider (self->simple, self);
}

static gboolean
nautilus_search_engine_start (NautilusSearchProvider *provider,
                              NautilusQuery          *query)
{
    g_return_val_if_fail (query != NULL, FALSE);

    g_autoptr (NautilusQuery) query_to_copy = g_object_ref (query);

    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (provider);

    g_debug ("Search engine start");
    guint num_finished = self->providers_error + self->providers_finished;

    g_clear_object (&self->query);
    self->query = nautilus_query_copy (query_to_copy);

    if (self->running)
    {
        if (num_finished == self->providers_running &&
            self->restart)
        {
            search_engine_start_real (self);
        }

        return TRUE;
    }

    self->running = TRUE;

    g_object_notify (G_OBJECT (self), "running");

    if (num_finished < self->providers_running)
    {
        self->restart = TRUE;
    }
    else
    {
        search_engine_start_real (self);
    }

    return TRUE;
}

static void
nautilus_search_engine_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (provider);

    g_debug ("Search engine stop");

    if (self->localsearch != NULL)
    {
        nautilus_search_provider_stop (self->localsearch);
    }
    if (self->model != NULL)
    {
        nautilus_search_provider_stop (self->model);
    }
    if (self->recent != NULL)
    {
        nautilus_search_provider_stop (self->recent);
    }
    if (self->simple != NULL)
    {
        nautilus_search_provider_stop (self->simple);
    }

    self->running = FALSE;
    self->restart = FALSE;

    g_object_notify (G_OBJECT (self), "running");
}

static void
search_provider_hits_added (NautilusSearchProvider *provider,
                            GPtrArray              *transferred_hits,
                            NautilusSearchEngine   *self)
{
    g_autoptr (GPtrArray) hits = transferred_hits;

    if (!self->running || self->restart)
    {
        g_debug ("Ignoring hits-added, since engine is %s",
                 !self->running ? "not running" : "waiting to restart");
        return;
    }

    g_autoptr (GPtrArray) added = g_ptr_array_new_with_free_func (g_object_unref);
    for (guint i = 0; i < hits->len; i++)
    {
        NautilusSearchHit *hit = hits->pdata[i];
        const char *uri = nautilus_search_hit_get_uri (hit);

        if (!g_hash_table_contains (self->uris, uri))
        {
            g_hash_table_add (self->uris, g_strdup (uri));
            g_ptr_array_add (added, g_object_ref (hit));
        }
    }

    if (added->len > 0)
    {
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (self),
                                             g_steal_pointer (&added));
    }
}

static void
check_providers_status (NautilusSearchEngine *self)
{
    guint num_finished;

    num_finished = self->providers_error + self->providers_finished;

    if (num_finished < self->providers_running)
    {
        return;
    }

    if (num_finished == self->providers_error)
    {
        g_debug ("Search engine error");
        nautilus_search_provider_error (NAUTILUS_SEARCH_PROVIDER (self),
                                        _("Unable to complete the requested search"));
    }
    else
    {
        if (self->restart)
        {
            g_debug ("Search engine finished and restarting");
        }
        else
        {
            g_debug ("Search engine finished");
        }
        nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (self),
                                           self->restart ? NAUTILUS_SEARCH_PROVIDER_STATUS_RESTARTING :
                                                           NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);
    }

    self->running = FALSE;
    g_object_notify (G_OBJECT (self), "running");

    g_hash_table_remove_all (self->uris);

    if (self->restart)
    {
        nautilus_search_engine_start (NAUTILUS_SEARCH_PROVIDER (self), self->query);
    }

    g_object_unref (self);
}

static void
search_provider_error (NautilusSearchProvider *provider,
                       const char             *error_message,
                       NautilusSearchEngine   *self)
{
    g_debug ("Search provider error: %s", error_message);

    self->providers_error++;

    check_providers_status (self);
}

static void
search_provider_finished (NautilusSearchProvider       *provider,
                          NautilusSearchProviderStatus  status,
                          NautilusSearchEngine         *self)
{
    g_debug ("Search provider finished");

    self->providers_finished++;

    check_providers_status (self);
}

typedef NautilusSearchProvider *(* CreateFunc) (void);

static void
setup_provider (NautilusSearchEngine    *self,
                NautilusSearchProvider **provider_pointer,
                NautilusSearchType       provider_flag,
                CreateFunc               create_func)
{
    if (self->search_type & provider_flag)
    {
        if (*provider_pointer == NULL)
        {
            *provider_pointer = create_func ();

            g_signal_connect (*provider_pointer, "hits-added",
                              G_CALLBACK (search_provider_hits_added),
                              self);
            g_signal_connect (*provider_pointer, "finished",
                              G_CALLBACK (search_provider_finished),
                              self);
            g_signal_connect (*provider_pointer, "error",
                              G_CALLBACK (search_provider_error),
                              self);
        }
    }
    else
    {
        g_clear_object (provider_pointer);
    }
}

void
nautilus_search_engine_set_search_type (NautilusSearchEngine *self,
                                        NautilusSearchType    search_type)
{
    if (self->search_type == search_type)
    {
        return;
    }

    self->search_type = search_type;

    setup_provider (self, &self->localsearch, NAUTILUS_SEARCH_TYPE_LOCALSEARCH,
                    (CreateFunc) nautilus_search_engine_localsearch_new);
    setup_provider (self, &self->model, NAUTILUS_SEARCH_TYPE_MODEL,
                    (CreateFunc) nautilus_search_engine_model_new);
    setup_provider (self, &self->recent, NAUTILUS_SEARCH_TYPE_RECENT,
                    (CreateFunc) nautilus_search_engine_recent_new);
    setup_provider (self, &self->simple, NAUTILUS_SEARCH_TYPE_SIMPLE,
                    (CreateFunc) nautilus_search_engine_simple_new);
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->start = nautilus_search_engine_start;
    iface->stop = nautilus_search_engine_stop;
}

static void
nautilus_search_engine_finalize (GObject *object)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (object);

    g_hash_table_destroy (self->uris);

    g_clear_object (&self->localsearch);
    g_clear_object (&self->recent);
    g_clear_object (&self->model);
    g_clear_object (&self->simple);
    g_clear_object (&self->query);

    G_OBJECT_CLASS (nautilus_search_engine_parent_class)->finalize (object);
}

static void
nautilus_search_engine_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (object);

    switch (prop_id)
    {
        case PROP_RUNNING:
        {
            g_value_set_boolean (value, self->running);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
search_engine_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (object);

    switch (prop_id)
    {
        case PROP_SEARCH_TYPE:
        {
            NautilusSearchType search_type = g_value_get_int (value);
            nautilus_search_engine_set_search_type (self, search_type);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_search_engine_class_init (NautilusSearchEngineClass *class)
{
    GObjectClass *object_class;

    object_class = (GObjectClass *) class;

    object_class->finalize = nautilus_search_engine_finalize;
    object_class->get_property = nautilus_search_engine_get_property;
    object_class->set_property = search_engine_set_property;

    /**
     * NautilusSearchEngine::running:
     *
     * Whether the search engine is running a search.
     */
    properties[PROP_RUNNING] =
        g_param_spec_boolean ("running",
                              "search running",
                              "Whether the engine is running a search",
                              FALSE,
                              G_PARAM_READABLE);
    properties[PROP_SEARCH_TYPE] =
        g_param_spec_int ("search-type",
                          "search type",
                          "a #NautilusSearchType",
                          0, G_MAXINT, 0,
                          G_PARAM_WRITABLE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_search_engine_init (NautilusSearchEngine *self)
{
    self->uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

NautilusSearchEngine *
nautilus_search_engine_new (NautilusSearchType search_type)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE,
                         "search-type", search_type,
                         NULL);
}
