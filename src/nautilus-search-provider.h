/*
 *  Copyright (C) 2012 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include "nautilus-query.h"
#include "nautilus-search-hit.h"

G_BEGIN_DECLS

typedef enum {
  NAUTILUS_SEARCH_PROVIDER_STATUS_NORMAL,
  NAUTILUS_SEARCH_PROVIDER_STATUS_RESTARTING
} NautilusSearchProviderStatus;

typedef enum {
  NAUTILUS_SEARCH_ENGINE_ALL_ENGINES,
  NAUTILUS_SEARCH_ENGINE_TRACKER_ENGINE,
  NAUTILUS_SEARCH_ENGINE_RECENT_ENGINE,
  NAUTILUS_SEARCH_ENGINE_MODEL_ENGINE,
  NAUTILUS_SEARCH_ENGINE_SIMPLE_ENGINE,
} NautilusSearchEngineTarget;

#define NAUTILUS_TYPE_SEARCH_PROVIDER (nautilus_search_provider_get_type ())

G_DECLARE_INTERFACE (NautilusSearchProvider, nautilus_search_provider, NAUTILUS, SEARCH_PROVIDER, GObject)

struct _NautilusSearchProviderInterface {
        GTypeInterface g_iface;

        /* VTable */
        void (*set_query) (NautilusSearchProvider *provider, NautilusQuery *query);
        void (*start) (NautilusSearchProvider *provider);
        void (*stop) (NautilusSearchProvider *provider);

        /* Signals */
        void (*hits_added) (NautilusSearchProvider *provider, GList *hits);
        /* This signal has a status parameter because it's necesary to discern
         * when the search engine finished normally or wheter it finished in a
         * different situation that will cause the engine to do some action after
         * finishing.
         *
         * For example, the search engine restarts itself if the client starts a
         * new search before all the search providers finished its current ongoing search.
         *
         * A real use case of this is when the user change quickly the query of the search,
         * the search engine stops all the search providers, but given that each search
         * provider has its own thread it will be actually stopped in a unknown time.
         * To fix that, the search engine marks itself for restarting if the client
         * starts a new search and not all providers finished. Then it will emit
         * its finished signal and restart all providers with the new search.
         *
         * That can cause that when the search engine emits its finished signal,
         * it actually relates to old searchs that it stopped and not the one
         * the client started lately.
         * The client doesn't have a way to know wheter the finished signal
         * relates to its current search or with an old search.
         *
         * To fix this situation, provide with the signal a status parameter, that
         * provides a hint of how the search engine stopped or if it is going to realize
         * some action afterwards, like restarting.
         */
        void (*finished) (NautilusSearchProvider       *provider,
                          NautilusSearchProviderStatus  status);
        void (*error) (NautilusSearchProvider *provider, const char *error_message);
        gboolean (*is_running) (NautilusSearchProvider *provider);
};

GType          nautilus_search_provider_get_type        (void) G_GNUC_CONST;

/* Interface Functions */
void           nautilus_search_provider_set_query       (NautilusSearchProvider *provider,
                                                         NautilusQuery *query);
void           nautilus_search_provider_start           (NautilusSearchProvider *provider);
void           nautilus_search_provider_stop            (NautilusSearchProvider *provider);

void           nautilus_search_provider_hits_added      (NautilusSearchProvider *provider,
                                                         GList *hits);
void           nautilus_search_provider_finished        (NautilusSearchProvider       *provider,
                                                         NautilusSearchProviderStatus  status);
void           nautilus_search_provider_error           (NautilusSearchProvider *provider,
                                                         const char *error_message);

gboolean       nautilus_search_provider_is_running      (NautilusSearchProvider *provider);

G_END_DECLS
