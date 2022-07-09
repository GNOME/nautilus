/*
 *  nautilus-property-page.h - Property pages exported by 
 *                             NautilusPropertyProvider objects.
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

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTY_PAGE (nautilus_property_page_get_type ())

G_DECLARE_FINAL_TYPE (NautilusPropertyPage, nautilus_property_page,
                      NAUTILUS, PROPERTY_PAGE,
                      GObject)

/**
 * SECTION:nautilus-property-page
 * @title: NautilusPropertyPage
 * @short_description: Property page descriptor object
 *
 * #NautilusPropertyPage is an object that describes a page in the file
 * properties dialog. Extensions can provide #NautilusPropertyPage objects
 * by registering a #NautilusPropertyPageProvider and returning them from
 * nautilus_property_page_provider_get_pages(), which will be called by the
 * main application when creating file properties dialogs.
 */

/**
 * nautilus_property_page_new:
 * @name: the identifier for the property page
 * @label: the user-visible label of the property page
 * @page: the property page to display
 *
 * Returns: (transfer full): a new #NautilusPropertyPage
 */
NautilusPropertyPage *nautilus_property_page_new (const char *name,
                                                  GtkWidget  *label,
                                                  GtkWidget  *page);

/* NautilusPropertyPage has the following properties:
 *   name (string)        - the identifier for the property page
 *   label (widget)       - the user-visible label of the property page
 *   page (widget)        - the property page to display
 */

G_END_DECLS
