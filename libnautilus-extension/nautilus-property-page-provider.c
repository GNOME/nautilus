/*
 *  nautilus-property-page-provider.c - Interface for Nautilus extensions
 *                                      that provide property pages for
 *                                      files.
 *
 *  Copyright (C) 2003 Novell, Inc.
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
 *
 */

#include "nautilus-property-page-provider.h"

G_DEFINE_INTERFACE (NautilusPropertyPageProvider, nautilus_property_page_provider,
                    G_TYPE_OBJECT)

static void
nautilus_property_page_provider_default_init (NautilusPropertyPageProviderInterface *klass)
{
}

GList *
nautilus_property_page_provider_get_pages (NautilusPropertyPageProvider *self,
                                           GList                        *files)
{
    NautilusPropertyPageProviderInterface *iface;

    g_return_val_if_fail (NAUTILUS_IS_PROPERTY_PAGE_PROVIDER (self), NULL);

    iface = NAUTILUS_PROPERTY_PAGE_PROVIDER_GET_IFACE (self);

    g_return_val_if_fail (iface->get_pages != NULL, NULL);

    return iface->get_pages (self, files);
}
