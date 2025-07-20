/*
 * Copyright (C) 2012 Red Hat, Inc.
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Peter Eisenmann <p3732@getgoogleoff.me>
 */
#define G_LOG_DOMAIN "nautilus-search"

#include <config.h>
#include "nautilus-search-provider.h"

#include "nautilus-enum-types.h"
#include "nautilus-query.h"

#include <gio/gio.h>
#include <glib-object.h>

typedef struct
{
    const char *name;
    guint delayed_timeout_id;

    GPtrArray *hits;
    guint batch_timeout_id;

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

static const char *
search_provider_name (NautilusSearchProvider *self)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);
    if (G_UNLIKELY (priv->name == NULL))
    {
        NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
        priv->name = klass->get_name (self);
    }

    return priv->name;
}

static void
actual_start (NautilusSearchProvider *self)
{
    NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    priv->delayed_timeout_id = 0;

    g_debug ("Search provider '%s' starting", search_provider_name (self));
    klass->start_search (self);
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
    priv->hits = g_ptr_array_new_with_free_func (g_object_unref);
    /* Keep reference on self while running */
    g_object_ref (self);

    guint delay_ms = klass->search_delay (self);
    if (delay_ms > 0)
    {
        g_debug ("Search provider '%s' delayed", search_provider_name (self));
        priv->delayed_timeout_id = g_timeout_add_once (delay_ms,
                                                       (GSourceOnceFunc) actual_start,
                                                       self);
    }
    else
    {
        actual_start (self);
    }

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

    NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    if (priv->delayed_timeout_id != 0)
    {
        g_debug ("Search provider '%s' cancelled before starting", search_provider_name (self));
        g_clear_handle_id (&priv->delayed_timeout_id, g_source_remove);

        nautilus_search_provider_finished (self);
    }
    else
    {
        g_debug ("Search provider '%s' stopping", search_provider_name (self));
        g_cancellable_cancel (priv->cancellable);

        klass->stop (self);
    }
}

static void
search_provider_submit_hits (NautilusSearchProvider *self,
                             GPtrArray              *hits)
{
    if (hits->len > 0)
    {
        g_debug ("Search provider '%s' found %d hits", search_provider_name (self), hits->len);
        g_signal_emit (self, signals[HITS_ADDED], 0, hits);
    }
}

static void
search_provider_submit_batch_idle (NautilusSearchProvider *self)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    priv->batch_timeout_id = 0;

    if (nautilus_search_provider_should_stop (self))
    {
        return;
    }


    /* Atomically swap compiled hits with new empty array */
    g_autoptr (GPtrArray) new_hits = g_ptr_array_new_with_free_func (g_object_unref);

    if (G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (&priv->hits, priv->hits, new_hits)))
    {
        g_debug ("Unlikely: compare and swap failed");
        return;
    }

    search_provider_submit_hits (self, g_steal_pointer (&new_hits));
}

void
nautilus_search_provider_finished (NautilusSearchProvider *self)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self));

    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    /* Flush any remaining hits */
    g_clear_handle_id (&priv->batch_timeout_id, g_source_remove);
    search_provider_submit_batch_idle (self);
    g_clear_pointer (&priv->hits, g_ptr_array_unref);

    g_clear_object (&priv->cancellable);
    g_clear_object (&priv->query);

    g_debug ("Search provider '%s' finished", search_provider_name (self));

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

void
nautilus_search_provider_add_hit (gpointer           self,
                                  NautilusSearchHit *hit)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    g_ptr_array_add (priv->hits, hit);

    if (priv->batch_timeout_id == 0)
    {
        /* Bundle all results found within a timeframe and sumbit them in one batch */
        guint batch_delay = 100;
        priv->batch_timeout_id = g_timeout_add_once (
            batch_delay, (GSourceOnceFunc) search_provider_submit_batch_idle, self);
    }
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
    g_clear_pointer (&priv->hits, g_ptr_array_unref);

    G_OBJECT_CLASS (nautilus_search_provider_parent_class)->dispose (object);
}

static gboolean
default_should_search (NautilusSearchProvider *provider,
                       NautilusQuery          *query)
{
    return TRUE;
}

static guint
default_search_delay (NautilusSearchProvider *provider)
{
    return 0;
}

static void
nautilus_search_provider_class_init (NautilusSearchProviderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = search_provider_dispose;

    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (klass);
    search_provider_class->should_search = default_should_search;
    search_provider_class->search_delay = default_search_delay;

    setup_signals ();
}
