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

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_PROVIDER (nautilus_search_provider_get_type ())

G_DECLARE_INTERFACE (NautilusSearchProvider, nautilus_search_provider, NAUTILUS, SEARCH_PROVIDER, GObject)

struct _NautilusSearchProviderInterface {
        GTypeInterface g_iface;

        /* VTable */
        /**
         * Returns: Whether search provider was started
         */
        gboolean (*start) (NautilusSearchProvider *provider,
                           NautilusQuery          *query);
        void (*stop) (NautilusSearchProvider *provider);

        /* Signals */
        /**
         * @provider: search provider
         * @hits: (transfer full): list of #NautilusSearchHit
         *
         * Provider emits this signal when adding search hits
         */
        void (*hits_added) (NautilusSearchProvider *provider, GPtrArray *hits);
        /**
         * @provider: search provider
         *
         * Provider emits this signal when it finished
         */
        void (*provider_finished) (NautilusSearchProvider *provider);
};

GType          nautilus_search_provider_get_type        (void) G_GNUC_CONST;

/* Interface Functions */
gboolean       nautilus_search_provider_start           (NautilusSearchProvider *provider,
                                                         NautilusQuery *query);
void           nautilus_search_provider_stop            (NautilusSearchProvider *provider);

void           nautilus_search_provider_hits_added      (NautilusSearchProvider *provider,
                                                         GPtrArray              *hits);

void           nautilus_search_provider_finished        (NautilusSearchProvider *provider);

G_END_DECLS
