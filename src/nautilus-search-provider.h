/*
 * Copyright (C) 2012 Red Hat, Inc.
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
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

        /** Name for provider implementation. Evaluated once. */
        const char * (*get_name) (NautilusSearchProvider *provider);
        /** Whether search happens in a dedicated thread. Optional for main thread. */
        gboolean (*run_in_thread) (NautilusSearchProvider *provider);
        /** Delay duration in ms. Optional. */
        guint (*search_delay) (NautilusSearchProvider *provider);

        /** Whether a search for given @query should run. Optional. */
        gboolean (*should_search) (NautilusSearchProvider *provider,
                                   NautilusQuery          *query);
        /** Starts a search. Only called when should_search() returned TRUE. */
        void (*start_search) (NautilusSearchProvider *provider);
        /** Stop an ongoing search. Optional. */
        void (*stop_search) (NautilusSearchProvider *provider);
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
NautilusQuery *
nautilus_search_provider_get_query (gpointer self);
void
nautilus_search_provider_add_hit (gpointer           self,
                                  NautilusSearchHit *hit);
void
nautilus_search_provider_flush_hits (gpointer self);

void           nautilus_search_provider_finished        (NautilusSearchProvider *provider);

G_END_DECLS
