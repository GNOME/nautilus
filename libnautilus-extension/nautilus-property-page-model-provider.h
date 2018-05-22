/*
 *  nautilus-property-page-provider.h - Interface for Nautilus extensions
 *                                      that provide property pages.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *  Copyright (C) 2018 Red Hat, Inc.
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
 *           Carlos Soriano <csoriano@redhat.com>
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
#include "nautilus-property-page-model.h"
#include "nautilus-extension-types.h"
#include "nautilus-file-info.h"


G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTY_PAGE_MODEL_PROVIDER (nautilus_property_page_model_provider_get_type ())

G_DECLARE_INTERFACE (NautilusPropertyPageModelProvider, nautilus_property_page_model_provider,
                     NAUTILUS, PROPERTY_PAGE_MODEL_PROVIDER,
                     GObject)

/**
 * NautilusPropertyPageModelProvider:
 * @g_iface: The parent interface.
 * @get_model: Returns #NautilusPropertyPageModel.
 *   See nautilus_property_page_provider_get_model() for details.
 *
 * Interface for extensions to provide additional a property page.
 */
struct _NautilusPropertyPageModelProviderInterface
{
    GTypeInterface parent;

    NautilusPropertyPageModel *(*get_model) (NautilusPropertyPageModelProvider *provider,
                                             GList                             *files);
};

/* Interface Functions */
NautilusPropertyPageModel *nautilus_property_page_model_provider_get_model (NautilusPropertyPageModelProvider *provider,
                                                                            GList                             *files);

G_END_DECLS
