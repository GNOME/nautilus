/*
 *  nautilus-info-provider.h - Interface for Nautilus extensions that 
 *                             provide info about files.
 *
 *  Copyright (C) 2003 Novell, Inc.
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
 *  Author:  Dave Camp <dave@ximian.com>
 *           Alexander Larsson <alexl@redhat.com>
 *
 */

/* This interface is implemented by Nautilus extensions that want to 
 * provide extra location widgets for a particular location.
 * Extensions are called when Nautilus displays a location.
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include <gtk/gtk.h>
/* This should be removed at some point. */
#include "nautilus-extension-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_LOCATION_WIDGET_PROVIDER (nautilus_location_widget_provider_get_type ())

G_DECLARE_INTERFACE (NautilusLocationWidgetProvider, nautilus_location_widget_provider,
                     NAUTILUS, LOCATION_WIDGET_PROVIDER,
                     GObject)

/**
 * SECTION:nautilus-location-widget-provider
 * @title: NautilusLocationWidgetProvider
 * @short_description: Interface to provide additional location widgets
 *
 * #NautilusLocationWidgetProvider allows extension to provide additional location
 * widgets in the file manager views.
 */

/**
 * NautilusLocationWidgetProviderInterface:
 * @g_iface: The parent interface.
 * @get_widget: Returns a #GtkWidget.
 *   See nautilus_location_widget_provider_get_widget() for details.
 *
 * Interface for extensions to provide additional location widgets.
 */
struct _NautilusLocationWidgetProviderInterface
{
    GTypeInterface g_iface;

    GtkWidget *(*get_widget) (NautilusLocationWidgetProvider *provider,
                              const char                     *uri,
                              GtkWidget                      *window);
};

/**
 * nautilus_location_widget_provider_get_widget:
 * @provider: a #NautilusLocationWidgetProvider
 * @uri: the URI of the location
 * @window: parent #GtkWindow
 *
 * Returns: (transfer none) (nullable): the location widget for @provider at @uri
 */
GtkWidget *nautilus_location_widget_provider_get_widget (NautilusLocationWidgetProvider *provider,
                                                         const char                     *uri,
                                                         GtkWidget                      *window);

G_END_DECLS
