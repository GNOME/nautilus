/*
 * Copyright (C) 2012 Red Hat, Inc.
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
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
        guint (*search_delay) (NautilusSearchProvider *provider);

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
void
nautilus_search_provider_add_hit (gpointer           self,
                                  NautilusSearchHit *hit);
void
nautilus_search_provider_finished (NautilusSearchProvider *provider);

G_END_DECLS
