/*
 *  nautilus-property-page-provider.h - Interface for Nautilus extensions
 *                                      that provide property pages.
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

/* This interface is implemented by Nautilus extensions that want to 
 * add property page to property dialogs.  Extensions are called when 
 * Nautilus needs property pages for a selection.  They are passed a 
 * list of NautilusFileInfo objects for which information should
 * be displayed  */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
/* These should be removed at some point. */
#include "nautilus-extension-types.h"
#include "nautilus-file-info.h"
#include "nautilus-property-page.h"


G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER (nautilus_property_page_provider_get_type ())

G_DECLARE_INTERFACE (NautilusPropertyPageProvider, nautilus_property_page_provider,
                     NAUTILUS, PROPERTY_PAGE_PROVIDER,
                     GObject)

/* For compatibility reasons, remove this once you start introducing breaking changes. */
/**
 * NautilusPropertyPageProviderIface: (skip)
 */
typedef NautilusPropertyPageProviderInterface NautilusPropertyPageProviderIface;

/**
 * SECTION:nautilus-property-page-provider
 * @title: NautilusPropertyPageProvider
 * @short_description: Interface to provide additional property pages
 *
 * #NautilusPropertyPageProvider allows extension to provide additional pages
 * for the file properties dialog.
 */

/**
 * NautilusPropertyPageProviderInterface:
 * @g_iface: The parent interface.
 * @get_pages: Returns a #GList of #NautilusPropertyPage.
 *   See nautilus_property_page_provider_get_pages() for details.
 *
 * Interface for extensions to provide additional property pages.
 */
struct _NautilusPropertyPageProviderInterface
{
    GTypeInterface g_iface;

    GList *(*get_pages) (NautilusPropertyPageProvider *provider,
                         GList                        *files);
};

/**
 * nautilus_property_page_provider_get_pages:
 * @provider: a #NautilusPropertyPageProvider
 * @files: (element-type NautilusFileInfo): a #GList of #NautilusFileInfo
 *
 * This function is called by Nautilus when it wants property page
 * items from the extension.
 *
 * This function is called in the main thread before a property page
 * is shown, so it should return quickly.
 *
 * Returns: (nullable) (element-type NautilusPropertyPage) (transfer full): A #GList of allocated #NautilusPropertyPage items.
 */
GList *nautilus_property_page_provider_get_pages (NautilusPropertyPageProvider *provider,
                                                  GList                        *files);

G_END_DECLS
