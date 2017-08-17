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

enum
{
    HITS_ADDED,
    FINISHED,
    ERROR,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (NautilusSearchProvider, nautilus_search_provider, G_TYPE_OBJECT)

static void
nautilus_search_provider_default_init (NautilusSearchProviderInterface *iface)
{
    /**
     * NautilusSearchProvider::running:
     *
     * Whether the provider is running a search.
     */
    g_object_interface_install_property (iface,
                                         g_param_spec_boolean ("running",
                                                               "Whether the provider is running",
                                                               "Whether the provider is running a search",
                                                               FALSE,
                                                               G_PARAM_READABLE));

    signals[HITS_ADDED] = g_signal_new ("hits-added",
                                        NAUTILUS_TYPE_SEARCH_PROVIDER,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (NautilusSearchProviderInterface, hits_added),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__POINTER,
                                        G_TYPE_NONE, 1,
                                        G_TYPE_POINTER);

    signals[FINISHED] = g_signal_new ("finished",
                                      NAUTILUS_TYPE_SEARCH_PROVIDER,
                                      G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET (NautilusSearchProviderInterface, finished),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__ENUM,
                                      G_TYPE_NONE, 1,
                                      NAUTILUS_TYPE_SEARCH_PROVIDER_STATUS);

    signals[ERROR] = g_signal_new ("error",
                                   NAUTILUS_TYPE_SEARCH_PROVIDER,
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET (NautilusSearchProviderInterface, error),
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__STRING,
                                   G_TYPE_NONE, 1,
                                   G_TYPE_STRING);
}

void
nautilus_search_provider_set_query (NautilusSearchProvider *provider,
                                    NautilusQuery          *query)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));
    g_return_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->set_query != NULL);
    g_return_if_fail (NAUTILUS_IS_QUERY (query));

    NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->set_query (provider, query);
}

void
nautilus_search_provider_start (NautilusSearchProvider *provider)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));
    g_return_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->start != NULL);

    NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->start (provider);
}

void
nautilus_search_provider_stop (NautilusSearchProvider *provider)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));
    g_return_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->stop != NULL);

    NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->stop (provider);
}

void
nautilus_search_provider_hits_added (NautilusSearchProvider *provider,
                                     GList                  *hits)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    g_signal_emit (provider, signals[HITS_ADDED], 0, hits);
}

void
nautilus_search_provider_finished (NautilusSearchProvider       *provider,
                                   NautilusSearchProviderStatus  status)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    g_signal_emit (provider, signals[FINISHED], 0, status);
}

void
nautilus_search_provider_error (NautilusSearchProvider *provider,
                                const char             *error_message)
{
    g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

    g_warning ("Provider %s failed with error %s\n",
               G_OBJECT_TYPE_NAME (provider), error_message);
    g_signal_emit (provider, signals[ERROR], 0, error_message);
}

gboolean
nautilus_search_provider_is_running (NautilusSearchProvider *provider)
{
    g_return_val_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider), FALSE);
    g_return_val_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->is_running, FALSE);

    return NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->is_running (provider);
}
