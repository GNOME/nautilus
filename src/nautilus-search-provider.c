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
#include "nautilus-query.h"

#include <gio/gio.h>
#include <glib-object.h>

typedef struct
{
    /* Thread-safe variables */
    GCancellable *cancellable;
    NautilusQuery *query;
} NautilusSearchProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusSearchProvider, nautilus_search_provider, G_TYPE_OBJECT)

enum
{
    HITS_ADDED,
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
setup_signals (void)
{
    /**
     * NautilusSearchProvider::hits-added:
     * @provider: the provider that found search hits
     * @hits: (transfer full): #GPtrArray of #NautilusSearchHit
     *
     * This signal is emitted when the provider has search hits.
     */
    signals[HITS_ADDED] = g_signal_new ("hits-added",
                                        NAUTILUS_TYPE_SEARCH_PROVIDER,
                                        G_SIGNAL_RUN_LAST, 0,
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__POINTER,
                                        G_TYPE_NONE, 1,
                                        G_TYPE_POINTER);

    /**
     * NautilusSearchProvider::provider-finished:
     * @provider: the provider that finished searching
     *
     * This signal is emitted when the provider finishes searching.
     */
    signals[FINISHED] = g_signal_new ("provider-finished", NAUTILUS_TYPE_SEARCH_PROVIDER,
                                      G_SIGNAL_RUN_LAST, 0,
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE, 0);
}

gboolean
nautilus_search_provider_start (NautilusSearchProvider *self,
                                NautilusQuery          *query)
{
    g_return_val_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self), FALSE);
    g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

    NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    /* Can't start provider again before it finished */
    g_return_val_if_fail (priv->cancellable == NULL, FALSE);

    if (!klass->should_search (self, query))
    {
        return FALSE;
    }

    priv->cancellable = g_cancellable_new ();
    g_set_object (&priv->query, query);
    /* Keep reference on self while running */
    g_object_ref (self);
    klass->start_search (self);

    return TRUE;
}

void
nautilus_search_provider_stop (NautilusSearchProvider *self)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self));

    if (nautilus_search_provider_should_stop (self))
    {
        return;
    }

    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    g_cancellable_cancel (priv->cancellable);

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
                                     GPtrArray              *hits)
{
    g_autoptr (GPtrArray) transferred_hits = hits;

    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (provider);

    if (g_cancellable_is_cancelled (priv->cancellable))
    {
        return;
    }

    g_signal_emit (provider, signals[HITS_ADDED], 0,
                   g_steal_pointer (&transferred_hits));
}

void
nautilus_search_provider_finished (NautilusSearchProvider *self)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self));

    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    g_clear_object (&priv->cancellable);
    g_clear_object (&priv->query);

    /* Drop self-reference, counterpart to start() */
    g_object_ref (self);

    g_signal_emit (self, signals[FINISHED], 0);
}

/** Protected methods, generic type for convenience */

gboolean
nautilus_search_provider_should_stop (gpointer self)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    return priv->cancellable == NULL || g_cancellable_is_cancelled (priv->cancellable);
}

GCancellable *
nautilus_search_provider_get_cancellable (gpointer self)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    return priv->cancellable;
}

NautilusQuery *
nautilus_search_provider_get_query (gpointer self)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    return priv->query;
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

    G_OBJECT_CLASS (nautilus_search_provider_parent_class)->dispose (object);
}

static gboolean
default_should_search (NautilusSearchProvider *provider,
                       NautilusQuery          *query)
{
    return TRUE;
}

static void
nautilus_search_provider_class_init (NautilusSearchProviderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (klass);

    object_class->dispose = search_provider_dispose;
    search_provider_class->should_search = default_should_search;

    setup_signals ();
}
