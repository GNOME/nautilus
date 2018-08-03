/*
 *  nautilus-location-widget-provider.c - Interface for Nautilus
 *                extensions that provide extra widgets for a location
 *
 *  Copyright (C) 2005 Red Hat, Inc.
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
 *  Author:  Alexander Larsson <alexl@redhat.com>
 *
 */

#include "nautilus-location-widget-provider.h"

G_DEFINE_INTERFACE (NautilusLocationWidgetProvider, nautilus_location_widget_provider,
                    G_TYPE_OBJECT)

static void
nautilus_location_widget_provider_default_init (NautilusLocationWidgetProviderInterface *klass)
{
}

GtkWidget *
nautilus_location_widget_provider_get_widget (NautilusLocationWidgetProvider *self,
                                              const char                     *uri,
                                              GtkWidget                      *window)
{
    NautilusLocationWidgetProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_LOCATION_WIDGET_PROVIDER (self), NULL);

    iface = NAUTILUS_LOCATION_WIDGET_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_widget != NULL, NULL);

    return iface->get_widget (self, uri, window);
}
