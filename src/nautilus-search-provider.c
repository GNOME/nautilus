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
 *
 */

#include <config.h>
#include "nautilus-search-provider.h"
#include "nautilus-enum-types.h"

#include <glib-object.h>

typedef struct
{
    GCancellable *cancellable;
} NautilusSearchProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusSearchProvider, nautilus_search_provider, G_TYPE_OBJECT)

enum
{
    HITS_ADDED,
    FINISHED,
    ERROR,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

gboolean
nautilus_search_provider_start (NautilusSearchProvider *self,
                                NautilusQuery          *query,
                                guint                   run_id)
{
    g_return_val_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self), FALSE);
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

    NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));

    return klass->start (self, query, run_id);
}

void
nautilus_search_provider_stop (NautilusSearchProvider *self)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self));

    NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));

    return klass->stop (self);
}

/**
 * nautilus_search_provider_hits_added:
 * @provider: search provider
 * @hits: (transfer full): list of #NautilusSearchHit
 */
void
nautilus_search_provider_hits_added (NautilusSearchProvider *provider,
                                     GPtrArray              *hits,
                                     guint                   run_id)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    g_signal_emit (provider, signals[HITS_ADDED], 0, hits, run_id);
}

void
nautilus_search_provider_finished (NautilusSearchProvider *provider,
                                   guint                   run_id)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    g_signal_emit (provider, signals[FINISHED], 0, FALSE, run_id);
}

void
nautilus_search_provider_error (NautilusSearchProvider *provider,
                                guint                   run_id)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    g_signal_emit (provider, signals[FINISHED], 0, TRUE, run_id);
}

static void
nautilus_search_provider_init (NautilusSearchProvider *self)
{
}

static void
search_provider_dispose (GObject *object)
{
    NautilusSearchProvider *self = NAUTILUS_SEARCH_PROVIDER (object);
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    g_clear_object (&priv->cancellable);
}

static void
nautilus_search_provider_class_init (NautilusSearchProviderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = search_provider_dispose;

    signals[HITS_ADDED] = g_signal_new ("hits-added",
                                        NAUTILUS_TYPE_SEARCH_PROVIDER,
                                        G_SIGNAL_RUN_LAST, 0,
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT);

    signals[FINISHED] = g_signal_new ("provider-finished", NAUTILUS_TYPE_SEARCH_PROVIDER,
                                      G_SIGNAL_RUN_LAST, 0,
                                      NULL, NULL, NULL,
                                      G_TYPE_NONE, 2, G_TYPE_BOOLEAN, G_TYPE_UINT);
}
