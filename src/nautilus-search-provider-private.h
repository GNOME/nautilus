/*
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */

#pragma once

#include "nautilus-search-provider.h"

#include <gio/gio.h>
#include <glib-object.h>

struct _NautilusSearchProviderClass
{
    GObjectClass parent_class;

    /* VTable */
    /**
     * Returns: Whether search provider was started
     */
    gboolean (*start) (NautilusSearchProvider *provider,
                       NautilusQuery          *query,
                       guint                   run_id);
    void (*stop) (NautilusSearchProvider *provider);

    /* Signals */
    /**
     * @provider: search provider
     * @hits: (transfer full): list of #NautilusSearchHit
     * @run_id: run ID that yielded the results
     *
     * Provider emits this signal when adding search hits
     */
    void (*hits_added) (NautilusSearchProvider *provider,
                        GPtrArray *hits,
                        guint run_id);
    /**
     * @provider: search provider
     * @with_error: whether provider ran into an error
     */
    void (*provider_finished) (NautilusSearchProvider *provider,
                               gboolean                with_error,
                               guint                   run_id);
};

struct _NautilusSearchProvider
{
    GObject parent;

    GCancellable *cancellable;
};

/*
static inline GCancellable *
nautilus_search_provider_get_cancellable (NautilusSearchProvider *self)
{
    return self->cancellable;
}

gboolean
nautilus_search_provider_should_stop (NautilusSearchProvider *self)
{
    return g_cancellable_is_cancelled (self->cancellable);    
}
*/
