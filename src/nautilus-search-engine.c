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
#include "nautilus-search-engine-model.h"
#include <glib/gi18n.h>
#include "nautilus-search-engine-localsearch.h"
#include "nautilus-search-engine-recent.h"
#include "nautilus-search-engine-simple.h"

struct _NautilusSearchEngine
{
    GObject parent_instance;

    NautilusSearchType search_type;

    NautilusSearchEngineLocalsearch *localsearch;
    NautilusSearchEngineRecent *recent;
    NautilusSearchEngineSimple *simple;
    NautilusSearchEngineModel *model;

    GHashTable *uris;
    guint providers_running;
    guint providers_finished;
    guint providers_error;

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
nautilus_search_engine_set_query (NautilusSearchProvider *provider,
                                  NautilusQuery          *query)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (provider);

    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (self->localsearch), query);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (self->recent), query);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (self->model), query);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (self->simple), query);
}

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
search_engine_start_real (NautilusSearchEngine *self)
{
    search_engine_start_real_setup (self);

    if (self->search_type & NAUTILUS_SEARCH_TYPE_LOCALSEARCH)
    {
        self->providers_running++;
        nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (self->localsearch));
    }

    if (self->search_type & NAUTILUS_SEARCH_TYPE_RECENT)
    {
        self->providers_running++;
        nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (self->recent));
    }

    if (self->search_type & NAUTILUS_SEARCH_TYPE_MODEL &&
        nautilus_search_engine_model_get_model (self->model) != NULL)
    {
        self->providers_running++;
        nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (self->model));
    }

    if (self->search_type & NAUTILUS_SEARCH_TYPE_SIMPLE)
    {
        self->providers_running++;
        nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (self->simple));
    }
}

static void
nautilus_search_engine_start (NautilusSearchProvider *provider)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (provider);

    g_debug ("Search engine start");
    guint num_finished = self->providers_error + self->providers_finished;

    if (self->running)
    {
        if (num_finished == self->providers_running &&
            self->restart)
        {
            search_engine_start_real (self);
        }

        return;
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
}

static void
nautilus_search_engine_stop (NautilusSearchProvider *provider)
{
    NautilusSearchEngine *self = NAUTILUS_SEARCH_ENGINE (provider);

    g_debug ("Search engine stop");

    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (self->localsearch));
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (self->recent));
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (self->model));
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (self->simple));

    self->running = FALSE;
    self->restart = FALSE;

    g_object_notify (G_OBJECT (self), "running");
}

static void
search_provider_hits_added (NautilusSearchProvider *provider,
                            GList                  *hits,
                            NautilusSearchEngine   *self)
{
    GList *added = NULL;
    GList *l;

    if (!self->running || self->restart)
    {
        g_debug ("Ignoring hits-added, since engine is %s",
                 !self->running ? "not running" : "waiting to restart");
        return;
    }

    for (l = hits; l != NULL; l = l->next)
    {
        NautilusSearchHit *hit = l->data;
        int count;
        const char *uri;

        uri = nautilus_search_hit_get_uri (hit);
        count = GPOINTER_TO_INT (g_hash_table_lookup (self->uris, uri));
        if (count == 0)
        {
            added = g_list_prepend (added, hit);
        }
        g_hash_table_replace (self->uris, g_strdup (uri), GINT_TO_POINTER (++count));
    }
    if (added != NULL)
    {
        added = g_list_reverse (added);
        nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (self), added);
        g_list_free (added);
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
        nautilus_search_engine_start (NAUTILUS_SEARCH_PROVIDER (self));
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

static void
connect_provider_signals (NautilusSearchEngine   *engine,
                          NautilusSearchProvider *provider)
{
    g_signal_connect (provider, "hits-added",
                      G_CALLBACK (search_provider_hits_added),
                      engine);
    g_signal_connect (provider, "finished",
                      G_CALLBACK (search_provider_finished),
                      engine);
    g_signal_connect (provider, "error",
                      G_CALLBACK (search_provider_error),
                      engine);
}

void
nautilus_search_engine_set_search_type (NautilusSearchEngine *self,
                                        NautilusSearchType    search_type)
{
    if (self->search_type != search_type)
    {
        self->search_type = search_type;
    }
}

static void
nautilus_search_provider_init (NautilusSearchProviderInterface *iface)
{
    iface->set_query = nautilus_search_engine_set_query;
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

    self->localsearch = nautilus_search_engine_localsearch_new ();
    connect_provider_signals (self, NAUTILUS_SEARCH_PROVIDER (self->localsearch));

    self->model = nautilus_search_engine_model_new ();
    connect_provider_signals (self, NAUTILUS_SEARCH_PROVIDER (self->model));

    self->simple = nautilus_search_engine_simple_new ();
    connect_provider_signals (self, NAUTILUS_SEARCH_PROVIDER (self->simple));

    self->recent = nautilus_search_engine_recent_new ();
    connect_provider_signals (self, NAUTILUS_SEARCH_PROVIDER (self->recent));
}

NautilusSearchEngine *
nautilus_search_engine_new (NautilusSearchType search_type)
{
    return g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE,
                         "search-type", search_type,
                         NULL);
}

NautilusSearchEngineModel *
nautilus_search_engine_get_model_provider (NautilusSearchEngine *self)
{
    return self->model;
}
