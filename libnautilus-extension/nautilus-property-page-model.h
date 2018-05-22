/*
 *  nautilus-property-page-model.h - Property pages exported by
 *                             NautilusPropertyProvider objects.
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
 *
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTY_PAGE_MODEL (nautilus_property_page_model_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusPropertyPageModel, nautilus_property_page_model,
                          NAUTILUS, PROPERTY_PAGE_MODEL,
                          GObject)

struct _NautilusPropertyPageModelClass
{
    GObjectClass parent_class;
};

typedef struct
{
    int id;
    char* title;
} NautilusPropertyPageModelSection;

typedef struct
{
    int section_id;
    char* field;
    char* value;
} NautilusPropertyPageModelItem;

NautilusPropertyPageModel *nautilus_property_page_model_new (const char *title,
                                                             GList      *sections,
                                                             GList      *items);
char * nautilus_property_page_model_get_title (NautilusPropertyPageModel *self);
void nautilus_property_page_model_set_title (NautilusPropertyPageModel *self,
                                             const char                *title);
GList *nautilus_property_page_model_get_sections (NautilusPropertyPageModel *self);
void
nautilus_property_page_model_set_sections (NautilusPropertyPageModel *self,
                                           GList                     *sections);
GList *nautilus_property_page_model_get_items (NautilusPropertyPageModel *self);
void
nautilus_property_page_model_set_items (NautilusPropertyPageModel *self,
                                        GList                     *items);

/* NautilusPropertyPageModel has the following properties:
 *   label (string)                                        - the user-visible label of the property page
 *   sections (GList of NautilusPropertyPageModelSection)  - the sections
 *   items (GList of NautilusPropertyPageModelItem)        - the items
 */

G_END_DECLS
