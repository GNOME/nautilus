/*
 * Copyright (C) 2018 Canonical Ltd
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
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 *
 */

#include <config.h>
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine-recent.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-tag-manager.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct _NautilusSearchEngineRecent
{
    GObject parent_instance;

    gboolean running;
    NautilusQuery *query;
    GtkRecentManager *recent;
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

    g_clear_object (&self->query);

    G_OBJECT_CLASS (nautilus_search_engine_recent_parent_class)->finalize (object);
}

static void
nautilus_search_engine_recent_start (NautilusSearchProvider *provider)
{
    GList *items, *hits, *l;

    NautilusSearchEngineRecent *self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);

    g_return_if_fail (self->query);

    hits = NULL;
    items = gtk_recent_manager_get_items (self->recent);
    self->running = TRUE;

    for (l = items; l; l = l->next)
    {
        GtkRecentInfo *info = l->data;
        const gchar *name = gtk_recent_info_get_display_name (info);
        gdouble rank = nautilus_query_matches_string (self->query, name);

        if (rank > 0)
        {
            NautilusSearchHit *hit;
            GDateTime *gmodified, *gvisited;
            time_t modified, visited;

            hit = nautilus_search_hit_new (gtk_recent_info_get_uri (info));

            modified = gtk_recent_info_get_modified (info);
            visited = gtk_recent_info_get_visited (info);

            gmodified = g_date_time_new_from_unix_local (modified);
            gvisited = g_date_time_new_from_unix_local (visited);

            hits = g_list_prepend (hits, hit);

            g_date_time_unref (gmodified);
            g_date_time_unref (gvisited);
        }
    }

    nautilus_search_provider_hits_added (provider, hits);

    nautilus_search_provider_finished (provider,
                                       NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL);

    self->running = FALSE;

    g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
    g_list_free_full (hits, g_object_unref);
}

static void
nautilus_search_engine_recent_stop (NautilusSearchProvider *provider)
{
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
    NautilusSearchEngineRecent *self;

    self = NAUTILUS_SEARCH_ENGINE_RECENT (provider);

    return self->running;
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
}

static void
nautilus_search_engine_recent_init (NautilusSearchEngineRecent *self)
{
    self->recent = gtk_recent_manager_get_default ();
}
