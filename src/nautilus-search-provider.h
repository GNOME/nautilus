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

#include "nautilus-types.h"

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_PROVIDER (nautilus_search_provider_get_type ())
G_DECLARE_DERIVABLE_TYPE (NautilusSearchProvider, nautilus_search_provider,
                          NAUTILUS, SEARCH_PROVIDER, GObject)

struct _NautilusSearchProviderClass
{
        GObjectClass parent_class;

        /** Whether a search for given @query should run. Optional. */
        gboolean (*should_search) (NautilusSearchProvider *provider,
                                   NautilusQuery          *query);

        /**
         * Returns: Whether search provider was started
         */
        gboolean (*start) (NautilusSearchProvider *provider,
                           NautilusQuery          *query);
        void (*stop) (NautilusSearchProvider *provider);
};

/* Interface Functions */
gboolean       nautilus_search_provider_start           (NautilusSearchProvider *provider,
                                                         NautilusQuery *query);
void           nautilus_search_provider_stop            (NautilusSearchProvider *provider);

/*
 * Protected methods, generic type for convenience.
 * Can be called from main context or a single search thread.
 */
gboolean
nautilus_search_provider_should_stop (gpointer self);
GCancellable *
nautilus_search_provider_get_cancellable (gpointer self);

void           nautilus_search_provider_hits_added      (NautilusSearchProvider *provider,
                                                         GPtrArray              *hits);

void           nautilus_search_provider_finished        (NautilusSearchProvider *provider);

G_END_DECLS
