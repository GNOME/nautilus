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

#define NAUTILUS_TYPE_SEARCH_PROVIDER (nautilus_search_provider_get_type ())
G_DECLARE_DERIVABLE_TYPE (NautilusSearchProvider, nautilus_search_provider,
                          NAUTILUS, SEARCH_PROVIDER, GObject)

struct _NautilusSearchProviderClass
{
        GObjectClass parent_class;

        const char * (*get_name) (NautilusSearchProvider *provider);

        /**
         * @self: object derived from #NautilusSearchProvider
         * @query: a #NautilusQuery
         *
         * Returns: Whether search provider should run for @query
         */
        gboolean (*should_search) (NautilusSearchProvider *provider,
                                   NautilusQuery          *query);

        void (*start_search) (NautilusSearchProvider *provider);
        void (*stop) (NautilusSearchProvider *provider);
};

/* Interface Functions */
gboolean
nautilus_search_provider_start (NautilusSearchProvider *provider,
                                NautilusQuery          *query,
                                guint                   run_id);
void
nautilus_search_provider_stop (NautilusSearchProvider *provider);

/* Protected methods, generic type for convenience */

gboolean
nautilus_search_provider_should_stop (gpointer self);
GCancellable *
nautilus_search_provider_get_cancellable (gpointer self);
NautilusQuery *
nautilus_search_provider_get_query (gpointer self);

void           nautilus_search_provider_hits_added      (NautilusSearchProvider *provider,
                                                         GPtrArray              *hits);

void
nautilus_search_provider_finished (NautilusSearchProvider *provider);

G_END_DECLS
