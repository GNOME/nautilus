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
    guint submit_on_idle_id;
    GPtrArray *hits_to_submit;

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

    priv->cancellable = g_cancellable_new ();
    priv->hits = g_ptr_array_new_with_free_func (g_object_unref);
    /* Keep reference on self while running */
    g_object_ref (self);

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

    g_set_object (&priv->query, query);

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
        g_clear_object (&priv->query);
        g_signal_emit (self, signals[FINISHED], 0);
    }
    else
    {
        g_debug ("Search provider '%s' stopping", search_provider_name (self));
        g_cancellable_cancel (priv->cancellable);

        klass->stop_search (self);
    }
}

static void
search_provider_submit_hits (NautilusSearchProvider *self,
                             GPtrArray              *hits)
{
    if (nautilus_search_provider_should_stop (self) ||
        hits->len == 0)
    {
        g_ptr_array_unref (hits);
        return;
    }

    g_debug ("Search provider '%s' found %d hits", search_provider_name (self), hits->len);
    g_signal_emit (self, signals[HITS_ADDED], 0, g_steal_pointer (&hits));
}

static void
search_provider_submit_on_idle (NautilusSearchProvider *self)
{
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    priv->submit_on_idle_id = 0;
    search_provider_submit_hits (self, g_steal_pointer (&priv->hits_to_submit));
}

void
nautilus_search_provider_finished (NautilusSearchProvider *self)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (self));

    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    if (priv->submit_on_idle_id != 0)
    {
        g_clear_handle_id (&priv->submit_on_idle_id, g_source_remove);
        search_provider_submit_hits (self, g_steal_pointer (&priv->hits_to_submit));
    }
    search_provider_submit_hits (self, g_steal_pointer (&priv->hits));

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
    const guint BATCH_LIMIT = 100;

    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    g_ptr_array_add (priv->hits, hit);

    if (priv->hits->len >= BATCH_LIMIT)
    {
        nautilus_search_provider_flush_hits (self);
    }
}

void
nautilus_search_provider_flush_hits (gpointer self)
{
    NautilusSearchProviderClass *klass = NAUTILUS_SEARCH_PROVIDER_CLASS (G_OBJECT_GET_CLASS (self));
    NautilusSearchProviderPrivate *priv = nautilus_search_provider_get_instance_private (self);

    if (nautilus_search_provider_should_stop (self) ||
        priv->submit_on_idle_id != 0)
    {
        /* Search aborted or last batch is still pending, don't schedule another */
        return;
    }

    g_return_if_fail (priv->hits_to_submit == NULL);

    if (klass->run_in_thread (self))
    {
        /* Schedule submit from main context */
        priv->hits_to_submit = priv->hits;
        priv->submit_on_idle_id = g_idle_add_once ((GSourceOnceFunc) search_provider_submit_on_idle,
                                                   self);
    }
    else
    {
        search_provider_submit_hits (self, g_steal_pointer (&priv->hits));
    }

    priv->hits = g_ptr_array_new_with_free_func (g_object_unref);
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

    g_warn_if_fail (priv->submit_on_idle_id == 0);

    g_clear_object (&priv->cancellable);
    g_clear_object (&priv->query);
    g_clear_pointer (&priv->hits, g_ptr_array_unref);
    if (priv->delayed_timeout_id != 0)
    {
        g_clear_handle_id (&priv->delayed_timeout_id, g_source_remove);
    }

    G_OBJECT_CLASS (nautilus_search_provider_parent_class)->dispose (object);
}

static gboolean
default_should_search (NautilusSearchProvider *provider,
                       NautilusQuery          *query)
{
    return TRUE;
}

static gboolean
default_run_in_thread (NautilusSearchProvider *provider)
{
    return FALSE;
}

static guint
default_search_delay (NautilusSearchProvider *provider)
{
    return 0;
}

static void
default_stop_search (NautilusSearchProvider *provider)
{
}

static void
nautilus_search_provider_class_init (NautilusSearchProviderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusSearchProviderClass *search_provider_class = NAUTILUS_SEARCH_PROVIDER_CLASS (klass);

    object_class->dispose = search_provider_dispose;
    search_provider_class->run_in_thread = default_run_in_thread;
    search_provider_class->should_search = default_should_search;
    search_provider_class->search_delay = default_search_delay;
    search_provider_class->stop_search = default_stop_search;

    setup_signals ();
}
